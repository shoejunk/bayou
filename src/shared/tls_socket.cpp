#include "tls_socket.hpp"

#include <asio.hpp>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace bayou::tls
{
namespace
{
constexpr std::uint32_t MaxPacketBytes = 16U * 1024U * 1024U;
constexpr std::size_t FrameHeaderBytes = sizeof(std::uint32_t);

#ifdef BAYOU_DEVELOPMENT_TLS_DEFAULTS
constexpr std::string_view DefaultCaFile = "tls/dev-ca-cert.pem";
constexpr std::string_view DefaultCertificateFile = "tls/dev-server-cert.pem";
constexpr std::string_view DefaultPrivateKeyFile = "tls/dev-server-key.pem";
#else
constexpr std::string_view DefaultCertificateFile;
constexpr std::string_view DefaultPrivateKeyFile;
#endif

std::string defaultCaFile()
{
#ifdef BAYOU_DEVELOPMENT_TLS_DEFAULTS
    return std::string(DefaultCaFile);
#elif defined(_WIN32)
    std::vector<wchar_t> path(32768);
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length != 0 && length < path.size())
    {
        return (std::filesystem::path(std::wstring_view(path.data(), length)).parent_path() /
                "bayou-ca.pem")
            .string();
    }
    return "bayou-ca.pem";
#else
    std::error_code error;
    const std::filesystem::path executable = std::filesystem::read_symlink("/proc/self/exe", error);
    return error ? "bayou-ca.pem" : (executable.parent_path() / "bayou-ca.pem").string();
#endif
}

std::string environmentOr(const char* name, std::string fallback)
{
    if (const char* value = std::getenv(name); value != nullptr && *value != '\0')
    {
        return value;
    }
    return fallback;
}

[[noreturn]] void throwTlsError(std::string_view operation, int error)
{
    std::array<char, 256> details{};
    mbedtls_strerror(error, details.data(), details.size());
    throw std::runtime_error(std::string(operation) + ": " + details.data());
}

std::string tlsErrorText(int error)
{
    std::array<char, 256> details{};
    mbedtls_strerror(error, details.data(), details.size());
    return details.data();
}

void requireTls(int result, std::string_view operation)
{
    if (result != 0)
    {
        throwTlsError(operation, result);
    }
}

class TlsConfiguration
{
public:
    explicit TlsConfiguration(bool server)
    {
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&random);
        mbedtls_ssl_config_init(&config);
        mbedtls_x509_crt_init(&certificates);
        mbedtls_pk_init(&privateKey);

        try
        {
            constexpr std::string_view personalization = "bayou-tls";
            requireTls(
                mbedtls_ctr_drbg_seed(
                    &random,
                    mbedtls_entropy_func,
                    &entropy,
                    reinterpret_cast<const unsigned char*>(personalization.data()),
                    personalization.size()),
                "seed TLS random generator");
            requireTls(
                mbedtls_ssl_config_defaults(
                    &config,
                    server ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
                    MBEDTLS_SSL_TRANSPORT_STREAM,
                    MBEDTLS_SSL_PRESET_DEFAULT),
                "configure TLS");
            mbedtls_ssl_conf_min_tls_version(&config, MBEDTLS_SSL_VERSION_TLS1_2);
            mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &random);

            if (server)
            {
                const std::string certificateFile =
                    environmentOr("BAYOU_TLS_CERT_FILE", std::string(DefaultCertificateFile));
                const std::string privateKeyFile =
                    environmentOr("BAYOU_TLS_KEY_FILE", std::string(DefaultPrivateKeyFile));
                if (certificateFile.empty() || privateKeyFile.empty())
                {
                    throw std::runtime_error("release TLS certificate and key are not configured");
                }
                requireTls(
                    mbedtls_x509_crt_parse_file(&certificates, certificateFile.c_str()),
                    "load TLS certificate");
                requireTls(
                    mbedtls_pk_parse_keyfile(
                        &privateKey,
                        privateKeyFile.c_str(),
                        nullptr,
                        mbedtls_ctr_drbg_random,
                        &random),
                    "load TLS private key");
                requireTls(
                    mbedtls_ssl_conf_own_cert(&config, &certificates, &privateKey),
                    "configure TLS certificate");
            }
            else
            {
                const std::string caFile = environmentOr("BAYOU_TLS_CA_FILE", defaultCaFile());
                if (caFile.empty())
                {
                    throw std::runtime_error("release TLS CA bundle is not configured");
                }
                requireTls(
                    mbedtls_x509_crt_parse_file(&certificates, caFile.c_str()),
                    "load TLS CA bundle");
                mbedtls_ssl_conf_ca_chain(&config, &certificates, nullptr);
                mbedtls_ssl_conf_authmode(&config, MBEDTLS_SSL_VERIFY_REQUIRED);
            }
        }
        catch (...)
        {
            release();
            throw;
        }
    }

    ~TlsConfiguration()
    {
        release();
    }

    void release()
    {
        mbedtls_pk_free(&privateKey);
        mbedtls_x509_crt_free(&certificates);
        mbedtls_ssl_config_free(&config);
        mbedtls_ctr_drbg_free(&random);
        mbedtls_entropy_free(&entropy);
    }

    TlsConfiguration(const TlsConfiguration&) = delete;
    TlsConfiguration& operator=(const TlsConfiguration&) = delete;

    mbedtls_entropy_context entropy{};
    mbedtls_ctr_drbg_context random{};
    mbedtls_ssl_config config{};
    mbedtls_x509_crt certificates{};
    mbedtls_pk_context privateKey{};
};

class TlsSession
{
public:
    TlsSession() { mbedtls_ssl_init(&ssl); }
    ~TlsSession() { mbedtls_ssl_free(&ssl); }
    mbedtls_ssl_context ssl{};
};

std::array<std::uint8_t, FrameHeaderBytes> encodeSize(std::uint32_t size)
{
    return {
        static_cast<std::uint8_t>((size >> 24U) & 0xffU),
        static_cast<std::uint8_t>((size >> 16U) & 0xffU),
        static_cast<std::uint8_t>((size >> 8U) & 0xffU),
        static_cast<std::uint8_t>(size & 0xffU)};
}

std::uint32_t decodeSize(const std::array<std::uint8_t, FrameHeaderBytes>& bytes)
{
    return (static_cast<std::uint32_t>(bytes[0]) << 24U) |
        (static_cast<std::uint32_t>(bytes[1]) << 16U) |
        (static_cast<std::uint32_t>(bytes[2]) << 8U) |
        static_cast<std::uint32_t>(bytes[3]);
}

sf::Socket::Status statusFor(const asio::error_code& error)
{
    if (!error)
    {
        return sf::Socket::Status::Done;
    }
    if (error == asio::error::would_block || error == asio::error::try_again)
    {
        return sf::Socket::Status::NotReady;
    }
    if (error == asio::error::eof || error == asio::error::connection_reset ||
        error == asio::error::connection_aborted)
    {
        return sf::Socket::Status::Disconnected;
    }
    return sf::Socket::Status::Error;
}
}

class Socket::Impl
{
public:
    asio::io_context io;
    asio::ip::tcp::socket socket{io};
    std::shared_ptr<TlsConfiguration> configuration;
    std::unique_ptr<TlsSession> session;
    bool blocking = true;
    std::string serverName = environmentOr("BAYOU_TLS_SERVER_NAME", "localhost");
    std::array<std::uint8_t, FrameHeaderBytes> receiveHeader{};
    std::size_t receiveHeaderOffset = 0;
    std::vector<std::uint8_t> receivePayload;
    std::size_t receivePayloadOffset = 0;

    static int tlsSend(void* context, const unsigned char* data, std::size_t size)
    {
        auto& self = *static_cast<Impl*>(context);
        asio::error_code error;
        const std::size_t sent = self.socket.send(asio::buffer(data, size), 0, error);
        if (!error)
        {
            return static_cast<int>(sent);
        }
        if (error == asio::error::would_block || error == asio::error::try_again)
        {
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        }
        if (error == asio::error::connection_reset || error == asio::error::connection_aborted)
        {
            return MBEDTLS_ERR_NET_CONN_RESET;
        }
        return MBEDTLS_ERR_NET_SEND_FAILED;
    }

    static int tlsReceive(void* context, unsigned char* data, std::size_t size)
    {
        auto& self = *static_cast<Impl*>(context);
        asio::error_code error;
        const std::size_t received = self.socket.receive(asio::buffer(data, size), 0, error);
        if (!error)
        {
            return static_cast<int>(received);
        }
        if (error == asio::error::would_block || error == asio::error::try_again)
        {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }
        if (error == asio::error::eof)
        {
            return 0;
        }
        if (error == asio::error::connection_reset || error == asio::error::connection_aborted)
        {
            return MBEDTLS_ERR_NET_CONN_RESET;
        }
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }

    void prepareTls(std::shared_ptr<TlsConfiguration> newConfiguration, bool client)
    {
        configuration = std::move(newConfiguration);
        session = std::make_unique<TlsSession>();
        requireTls(mbedtls_ssl_setup(&session->ssl, &configuration->config), "initialize TLS session");
        if (client)
        {
            requireTls(mbedtls_ssl_set_hostname(&session->ssl, serverName.c_str()), "set TLS server name");
        }
        mbedtls_ssl_set_bio(&session->ssl, this, tlsSend, tlsReceive, nullptr);
    }

    int handshake(std::chrono::milliseconds timeout = std::chrono::seconds(10))
    {
        asio::error_code modeError;
        socket.non_blocking(true, modeError);
        if (modeError)
        {
            return MBEDTLS_ERR_NET_SOCKET_FAILED;
        }
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        int result = 0;
        while ((result = mbedtls_ssl_handshake(&session->ssl)) != 0)
        {
            if (result != MBEDTLS_ERR_SSL_WANT_READ && result != MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                return result;
            }
            if (std::chrono::steady_clock::now() >= deadline)
            {
                return MBEDTLS_ERR_SSL_TIMEOUT;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        socket.non_blocking(!blocking, modeError);
        return 0;
    }

    void resetReceive()
    {
        receiveHeaderOffset = 0;
        receivePayload.clear();
        receivePayloadOffset = 0;
    }

    sf::Socket::Status readSome(void* destination, std::size_t size, std::size_t& received)
    {
        const int result = mbedtls_ssl_read(
            &session->ssl,
            static_cast<unsigned char*>(destination),
            size);
        if (result > 0)
        {
            received = static_cast<std::size_t>(result);
            return sf::Socket::Status::Done;
        }
        received = 0;
        if (result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            return sf::Socket::Status::NotReady;
        }
        if (result == 0 || result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ||
            result == MBEDTLS_ERR_NET_CONN_RESET)
        {
            return sf::Socket::Status::Disconnected;
        }
        return sf::Socket::Status::Error;
    }
};

class Listener::Impl
{
public:
    asio::io_context io;
    asio::ip::tcp::acceptor acceptor{io};
    std::shared_ptr<TlsConfiguration> configuration;
    bool configurationValid = false;
    bool blocking = true;

    Impl()
    {
        try
        {
            configuration = std::make_shared<TlsConfiguration>(true);
            configurationValid = true;
        }
        catch (const std::exception& error)
        {
            std::cerr << "TLS server configuration failed: " << error.what() << '\n';
            configurationValid = false;
        }
    }
};

Socket::Socket() : impl(std::make_unique<Impl>()) {}
Socket::~Socket() { disconnect(); }
Socket::Socket(Socket&&) noexcept = default;
Socket& Socket::operator=(Socket&&) noexcept = default;

sf::Socket::Status Socket::connect(const sf::IpAddress& address, unsigned short port)
{
    disconnect();
    try
    {
        impl->configuration = std::make_shared<TlsConfiguration>(false);
        asio::error_code error;
        const asio::ip::address asioAddress = asio::ip::make_address(address.toString(), error);
        if (error)
        {
            disconnect();
            return sf::Socket::Status::Error;
        }
        impl->socket.connect({asioAddress, port}, error);
        if (error)
        {
            disconnect();
            return statusFor(error);
        }
        impl->prepareTls(impl->configuration, true);
        const int handshakeResult = impl->handshake();
        if (handshakeResult != 0)
        {
            std::cerr << "TLS client handshake failed: " << tlsErrorText(handshakeResult) << '\n';
            disconnect();
            return sf::Socket::Status::Error;
        }
        impl->socket.non_blocking(!impl->blocking, error);
        return statusFor(error);
    }
    catch (const std::exception& error)
    {
        std::cerr << "TLS client setup failed: " << error.what() << '\n';
        disconnect();
        return sf::Socket::Status::Error;
    }
}

sf::Socket::Status Socket::send(const sf::Packet& packet)
{
    if (!impl->session || !impl->socket.is_open())
    {
        return sf::Socket::Status::Disconnected;
    }
    if (packet.getDataSize() > MaxPacketBytes ||
        packet.getDataSize() > std::numeric_limits<std::uint32_t>::max())
    {
        return sf::Socket::Status::Error;
    }

    const auto header = encodeSize(static_cast<std::uint32_t>(packet.getDataSize()));
    asio::error_code error;
    impl->socket.non_blocking(false, error);
    if (error)
    {
        return statusFor(error);
    }

    const auto writeAll = [&](const unsigned char* data, std::size_t size) {
        std::size_t offset = 0;
        while (offset < size)
        {
            const int result = mbedtls_ssl_write(&impl->session->ssl, data + offset, size - offset);
            if (result > 0)
            {
                offset += static_cast<std::size_t>(result);
            }
            else if (result != MBEDTLS_ERR_SSL_WANT_READ && result != MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                return false;
            }
        }
        return true;
    };

    const bool sent = writeAll(header.data(), header.size()) &&
        writeAll(static_cast<const unsigned char*>(packet.getData()), packet.getDataSize());
    impl->socket.non_blocking(!impl->blocking, error);
    if (!sent)
    {
        return sf::Socket::Status::Error;
    }
    return statusFor(error);
}

sf::Socket::Status Socket::send(
    const sf::Packet& packet,
    std::chrono::milliseconds timeout)
{
    if (!impl->session || !impl->socket.is_open())
    {
        return sf::Socket::Status::Disconnected;
    }
    if (timeout <= std::chrono::milliseconds::zero() ||
        packet.getDataSize() > MaxPacketBytes ||
        packet.getDataSize() > std::numeric_limits<std::uint32_t>::max())
    {
        return sf::Socket::Status::Error;
    }

    const auto header = encodeSize(static_cast<std::uint32_t>(packet.getDataSize()));
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    asio::error_code error;
    impl->socket.non_blocking(true, error);
    if (error)
    {
        return statusFor(error);
    }

    bool timedOut = false;
    const auto writeAll = [&](const unsigned char* data, std::size_t size) {
        std::size_t offset = 0;
        while (offset < size)
        {
            const int result = mbedtls_ssl_write(&impl->session->ssl, data + offset, size - offset);
            if (result > 0)
            {
                offset += static_cast<std::size_t>(result);
                continue;
            }
            if (result != MBEDTLS_ERR_SSL_WANT_READ && result != MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                return false;
            }
            if (std::chrono::steady_clock::now() >= deadline)
            {
                timedOut = true;
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return true;
    };

    const bool sent = writeAll(header.data(), header.size()) &&
        writeAll(static_cast<const unsigned char*>(packet.getData()), packet.getDataSize());
    if (!sent)
    {
        // A partially written length-framed packet cannot be retried safely on
        // this connection. Closing it lets the higher layer reconnect/resume.
        disconnect();
        return timedOut ? sf::Socket::Status::NotReady : sf::Socket::Status::Error;
    }

    impl->socket.non_blocking(!impl->blocking, error);
    if (error)
    {
        disconnect();
        return sf::Socket::Status::Error;
    }
    return sf::Socket::Status::Done;
}

sf::Socket::Status Socket::receive(sf::Packet& packet)
{
    if (!impl->session || !impl->socket.is_open())
    {
        return sf::Socket::Status::Disconnected;
    }
    while (impl->receiveHeaderOffset < FrameHeaderBytes)
    {
        std::size_t count = 0;
        const sf::Socket::Status status = impl->readSome(
            impl->receiveHeader.data() + impl->receiveHeaderOffset,
            FrameHeaderBytes - impl->receiveHeaderOffset,
            count);
        impl->receiveHeaderOffset += count;
        if (status != sf::Socket::Status::Done)
        {
            return status;
        }
    }
    if (impl->receivePayload.empty())
    {
        const std::uint32_t payloadSize = decodeSize(impl->receiveHeader);
        if (payloadSize > MaxPacketBytes)
        {
            disconnect();
            return sf::Socket::Status::Error;
        }
        impl->receivePayload.resize(payloadSize);
    }
    while (impl->receivePayloadOffset < impl->receivePayload.size())
    {
        std::size_t count = 0;
        const sf::Socket::Status status = impl->readSome(
            impl->receivePayload.data() + impl->receivePayloadOffset,
            impl->receivePayload.size() - impl->receivePayloadOffset,
            count);
        impl->receivePayloadOffset += count;
        if (status != sf::Socket::Status::Done)
        {
            return status;
        }
    }
    packet.clear();
    if (!impl->receivePayload.empty())
    {
        packet.append(impl->receivePayload.data(), impl->receivePayload.size());
    }
    impl->resetReceive();
    return sf::Socket::Status::Done;
}

void Socket::disconnect()
{
    if (!impl)
    {
        return;
    }
    if (impl->session)
    {
        mbedtls_ssl_close_notify(&impl->session->ssl);
    }
    if (impl->socket.is_open())
    {
        asio::error_code error;
        impl->socket.shutdown(asio::ip::tcp::socket::shutdown_both, error);
        impl->socket.close(error);
    }
    impl->session.reset();
    impl->configuration.reset();
    impl->resetReceive();
}

void Socket::setBlocking(bool blocking)
{
    impl->blocking = blocking;
    if (impl->socket.is_open())
    {
        asio::error_code error;
        impl->socket.non_blocking(!blocking, error);
    }
}

bool Socket::isBlocking() const { return impl->blocking; }
std::optional<sf::IpAddress> Socket::getRemoteAddress() const
{
    if (!impl->socket.is_open())
    {
        return std::nullopt;
    }
    asio::error_code error;
    const auto endpoint = impl->socket.remote_endpoint(error);
    if (error)
    {
        return std::nullopt;
    }

    const asio::ip::address address = endpoint.address();
    if (address.is_v4())
    {
        return sf::IpAddress::resolve(address.to_string());
    }

    // The listener is dual-stack, so IPv4 peers normally appear as mapped
    // IPv6 addresses (for example ::ffff:127.0.0.1). SFML's IpAddress is
    // IPv4-only and cannot resolve that spelling directly.
    const asio::ip::address_v6 ipv6 = address.to_v6();
    if (ipv6.is_loopback())
    {
        return sf::IpAddress::LocalHost;
    }
    if (!ipv6.is_v4_mapped())
    {
        return std::nullopt;
    }

    const asio::ip::address_v6::bytes_type bytes = ipv6.to_bytes();
    return sf::IpAddress(bytes[12], bytes[13], bytes[14], bytes[15]);
}

unsigned short Socket::getRemotePort() const
{
    if (!impl->socket.is_open())
    {
        return 0;
    }
    asio::error_code error;
    const auto endpoint = impl->socket.remote_endpoint(error);
    return error ? 0 : endpoint.port();
}

void Socket::setServerName(std::string serverName) { impl->serverName = std::move(serverName); }

Listener::Listener() : impl(std::make_unique<Impl>()) {}
Listener::~Listener() { close(); }

sf::Socket::Status Listener::listen(unsigned short port)
{
    if (!impl->configurationValid)
    {
        return sf::Socket::Status::Error;
    }
    asio::error_code error;
    if (impl->acceptor.is_open())
    {
        impl->acceptor.close(error);
        error.clear();
    }
    impl->acceptor.open(asio::ip::tcp::v6(), error);
    if (!error)
    {
        impl->acceptor.set_option(asio::socket_base::reuse_address(true), error);
        impl->acceptor.set_option(asio::ip::v6_only(false), error);
        impl->acceptor.bind({asio::ip::tcp::v6(), port}, error);
    }
    if (error)
    {
        impl->acceptor.close();
        error.clear();
        impl->acceptor.open(asio::ip::tcp::v4(), error);
        if (!error)
        {
            impl->acceptor.set_option(asio::socket_base::reuse_address(true), error);
            impl->acceptor.bind({asio::ip::tcp::v4(), port}, error);
        }
    }
    if (!error)
    {
        impl->acceptor.listen(asio::socket_base::max_listen_connections, error);
    }
    if (!error)
    {
        impl->acceptor.non_blocking(!impl->blocking, error);
    }
    return statusFor(error);
}

sf::Socket::Status Listener::accept(Socket& socket)
{
    if (!impl->configurationValid || !impl->acceptor.is_open())
    {
        return sf::Socket::Status::Error;
    }
    socket.disconnect();
    asio::error_code error;
    impl->acceptor.accept(socket.impl->socket, error);
    if (error)
    {
        return statusFor(error);
    }
    try
    {
        socket.impl->prepareTls(impl->configuration, false);
        const int handshakeResult = socket.impl->handshake();
        if (handshakeResult != 0)
        {
            std::cerr << "TLS server handshake failed: " << tlsErrorText(handshakeResult) << '\n';
            socket.disconnect();
            return sf::Socket::Status::Error;
        }
        socket.impl->socket.non_blocking(!socket.impl->blocking, error);
        return statusFor(error);
    }
    catch (const std::exception& error)
    {
        std::cerr << "TLS server setup failed: " << error.what() << '\n';
        socket.disconnect();
        return sf::Socket::Status::Error;
    }
}

void Listener::setBlocking(bool blocking)
{
    impl->blocking = blocking;
    if (impl->acceptor.is_open())
    {
        asio::error_code error;
        impl->acceptor.non_blocking(!blocking, error);
    }
}

void Listener::close()
{
    if (impl && impl->acceptor.is_open())
    {
        asio::error_code error;
        impl->acceptor.close(error);
    }
}
}
