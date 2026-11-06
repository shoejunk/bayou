"""Decode, validate and export the compact ElevenLabs library without new generations."""
from pathlib import Path
import hashlib
import json
import math
import subprocess
import sys
import wave

import numpy as np

ROOT = Path.cwd()
AUDIT = ROOT / 'output/sfx-audit'
LIBRARY = ROOT / 'assets/audio/sfx'
sys.path.insert(0, str(AUDIT / 'audio-tools'))
import imageio_ffmpeg

FFMPEG = imageio_ffmpeg.get_ffmpeg_exe()
RATE = 44100

def ffmpeg(*args):
    return subprocess.run([FFMPEG, '-hide_banner', '-loglevel', 'error', *map(str,args)], check=True, capture_output=True).stdout

def decode(file):
    raw = ffmpeg('-i', file, '-f', 'f32le', '-ac', '1', '-ar', RATE, 'pipe:1')
    return np.frombuffer(raw, dtype='<f4').copy()

def db(value):
    return round(20 * math.log10(max(float(value), 1e-12)), 2)

def wav(file, samples):
    file.parent.mkdir(parents=True, exist_ok=True)
    data = np.rint(np.clip(samples,-1,1) * 32767).astype('<i2')
    with wave.open(str(file),'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE); w.writeframes(data.tobytes())

def fade(samples, milliseconds=8):
    x = samples.copy()
    n = min(round(milliseconds/1000*RATE), len(x)//4)
    if n:
        x[:n] *= np.linspace(0,1,n)
        x[-n:] *= np.linspace(1,0,n)
    return x

def strongest_peaks(x, count=1, gap=0.32):
    block=441
    energies=np.array([np.mean(np.square(x[i:i+block])) for i in range(0,len(x),block)])
    candidates=np.argsort(energies)[::-1]
    chosen=[]
    for index in candidates:
        t=int(index*block)
        if all(abs(t-other)>=gap*RATE for other in chosen): chosen.append(t)
        if len(chosen)>=count: break
    return sorted(chosen)

manifest=json.loads((LIBRARY/'manifest.json').read_text(encoding='utf-8-sig'))
provenance=json.loads((AUDIT/'generation-completed.json').read_text(encoding='utf-8-sig'))
metadata={m['id']:m for m in provenance['masters']}
manifest['flow_id']=provenance['flow_id']
manifest['estimated_cost']=provenance['estimate']
manifest['status']='complete'
manifest['format']={'sample_rate':RATE,'channels':1,'wav_bits':16,'peak_ceiling_dbfs':-3,'source_recordings':41}
for folder in ['wav','slices','demos']:(LIBRARY/folder).mkdir(exist_ok=True)
qa=[]
audio={}
samples_by_id={}
slice_ids={'foot_soft','foot_heavy','hoof_crystal','wing_feather','wing_insect','swamp_squelch','mechanism','blade','blunt','bow','gun','magic_bright','magic_dark','beast_bite','body_hit','metal_hit','wood_hit','crystal','heal','roots','teleport','power','potion','steam','explosion','shriek','charm_pipes','death','ghost_chorus'}
for master in manifest['masters']:
    ident=master['id']; source=ROOT/master['file']
    if not source.is_file():raise RuntimeError(f'Missing recording: {ident}')
    state=metadata[ident]
    if not master['reuse_source'] and state['status']!='completed':raise RuntimeError(f'Unfinished generation: {ident}')
    x=decode(source)
    if len(x)<RATE*0.2 or not np.all(np.isfinite(x)):raise RuntimeError(f'Invalid audio samples: {ident}')
    peak=float(np.max(np.abs(x))); rms=float(np.sqrt(np.mean(x*x)))
    if peak<0.001 or rms<0.00005:raise RuntimeError(f'Silent audio: {ident}')
    # The generated ambience is deliberately very quiet; normalize that bed by
    # RMS so it is audible in the audition, while preserving its peak headroom.
    gain=min(10**(-6/20)/peak,10**(-27/20)/rms) if ident=='bayou' else min(10**(12/20),10**(-3/20)/peak)
    y=x*gain
    loop=master.get('loop',False)
    if loop:
        n=min(round(RATE*.15),len(y)//8)
        w=np.linspace(0,1,n)
        cross=y[-n:]*(1-w)+y[:n]*w
        y=np.concatenate([y[n:-n],cross]).astype('float32')
    else:y=fade(y)
    target=ROOT/master['wav'];wav(target,y)
    check=decode(target)
    if float(np.max(np.abs(check)))>0.709:raise RuntimeError(f'Headroom failure: {ident}')
    master.update({k:v for k,v in state.items() if v is not None and k not in ['status','error']})
    master['status']='complete';master['duration_actual']=round(len(x)/RATE,4);master['wav_duration']=round(len(y)/RATE,4)
    master['sha256']=hashlib.sha256(source.read_bytes()).hexdigest()
    master['wav_sha256']=hashlib.sha256(target.read_bytes()).hexdigest()
    master['normalization_gain_db']=db(gain)
    master['loop_export_crossfade_ms']=150 if loop else 0
    master['slices']=[]
    if ident in slice_ids:
        multiple=ident in {'foot_soft','foot_heavy','hoof_crystal','wing_feather','swamp_squelch','mechanism'}
        count=3 if ident=='hoof_crystal' else 2 if multiple else 1
        span=.36 if multiple else .42
        for j,peak_at in enumerate(strongest_peaks(y,count, gap=.42 if ident=='hoof_crystal' else .32)):
            start=max(0,peak_at-round(.10*RATE))
            end=min(len(y),start+round(span*RATE))
            snippet=fade(y[start:end],6)
            file=LIBRARY/'slices'/f'{ident}_{j+1:02d}.wav';wav(file,snippet)
            master['slices'].append({'file':file.relative_to(ROOT).as_posix(),'offset_seconds':round(start/RATE,4),'duration_seconds':round(len(snippet)/RATE,4),'transient_seconds':round((peak_at-start)/RATE,4)})
    audio[ident]=y
    samples_by_id[ident]=y
    qa.append({'id':ident,'source_duration':round(len(x)/RATE,4),'source_peak_dbfs':db(peak),'source_rms_dbfs':db(rms),'wav_duration':master['wav_duration'],'export_peak_dbfs':db(np.max(np.abs(check))),'export_rms_dbfs':db(np.sqrt(np.mean(check*check))),'samples':len(check),'sha256':master['sha256'],'wav_sha256':master['wav_sha256'],'slice_count':len(master['slices']),'finite':True,'decoder_ok':True,'not_silent':True})
    print('Exported:',ident,flush=True)

def short(ident):
    m=next(m for m in manifest['masters'] if m['id']==ident)
    return decode(ROOT/m['slices'][0]['file']) if m['slices'] else audio[ident]

demos=[
 ('bull_gator_attack','Bull Gator attack',[('beast_bite',1,0),('swamp_squelch',.18,0)]),
 ('crystal_unicorn_attack','Crystal Unicorn prismatic attack',[('magic_bright',.75,0),('crystal',.35,.08)]),
 ('thaeron_attack','Thaeron magic and blade attack',[('magic_dark',.85,0),('blade',.2,.04)]),
 ('gearjaw_attack','Gearjaw mechanical bite',[('mechanism',.65,0),('metal_hit',.55,.10)]),
 ('sylvara_bind','Sylvara roots and thorns',[('roots',.75,0),('magic_bright',.35,.04)]),
 ('choir_hex','The Choir spectral hex',[('ghost_chorus',.8,0),('magic_dark',.28,.04)]),
 ('pavo_charm','Pavo enchanting pipes',[('charm_pipes',.8,0),('crystal',.15,.12)]),
 ('keg_detonation','Black Powder Keg',[('fire',.20,0),('explosion',.8,.15)])
]
manifest['demonstrations']=[]
for ident,label,layers in demos:
    # Audition composites are derivatives of existing masters, not extra generated recordings.
    parts=[(short(sound),gain,int(at*RATE)) for sound,gain,at in layers]
    mixed=np.zeros(max(len(x)+at for x,gain,at in parts),dtype='float32')
    for x,gain,at in parts:mixed[at:at+len(x)]+=x*gain
    if np.max(np.abs(mixed))>.708:mixed*=.708/np.max(np.abs(mixed))
    file=LIBRARY/'demos'/f'{ident}.wav';wav(file,fade(mixed,8))
    manifest['demonstrations'].append({'id':ident,'name':label,'file':file.relative_to(ROOT).as_posix(),'layers':[{'id':i,'gain':g,'delay_seconds':t} for i,g,t in layers]})

silence=np.zeros(round(.40*RATE),dtype='float32')
reel=[];seconds=0;chapters=[]
for master in manifest['masters']:
    y=audio[master['id']]
    limit=4 if master['loop'] else 5
    y=fade(y[:int(limit*RATE)],12)
    chapters.append({'number':master['number'],'id':master['id'],'name':master['name'],'start_seconds':round(seconds,3),'duration_seconds':round(len(y)/RATE,3)})
    reel.extend([y,silence]);seconds+=(len(y)+len(silence))/RATE
wav(LIBRARY/'audition-reel.wav',np.concatenate(reel))
ffmpeg('-y','-i',LIBRARY/'audition-reel.wav','-codec:a','libmp3lame','-b:a','192k',LIBRARY/'audition-reel.mp3')
manifest['audition_reel']={'mp3':'assets/audio/sfx/audition-reel.mp3','wav':'assets/audio/sfx/audition-reel.wav','duration_seconds':round(seconds,3),'chapters':chapters}
(LIBRARY/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
(AUDIT/'audio-qa.json').write_text(json.dumps({'passed':True,'master_count':len(qa),'checks':qa,'scope':'Decoder, samples, silence, peak, duration and hashes only; semantic and in-game listening review remains a separate activity.'},indent=2)+'\n',encoding='utf-8')
(AUDIT/'audition-chapters.json').write_text(json.dumps(chapters,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'masters':len(qa),'slices':sum(q['slice_count'] for q in qa),'demonstrations':len(demos),'audition_seconds':round(seconds,3),'qa_passed':True}),flush=True)
