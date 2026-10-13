# Processes the four privately supplied ElevenLabs PCM inputs in assets/audio.
# They are not included in a source checkout and must never enter public Git history.
# Does not call the paid generation API again. Python standard library only.
# Source: elevenlabs text-to-sound-effects convert, eleven_text_to_sound_v2,
# duration-seconds=0.5, prompt-influence=0.7, output-format=pcm_24000, no-retry.
# Returned 46080 bytes per cue. Channel layout is NOT authoritatively confirmed.
# Provisional stereo PCM16 LE interpretation uses signal analysis, not metadata.
# Original .pcm bytes are preserved; no claim of exact original duration/timbre.
# No private/game/player data was sent. Prompts and measurements are in provenance.json.
$ErrorActionPreference = 'Stop'
$root=Split-Path -Parent $PSScriptRoot
foreach($cue in @('obstacle','person','object','destination')){
    if(-not(Test-Path -LiteralPath (Join-Path $root "assets\audio\$cue.pcm"))){
        throw 'Private PCM inputs are not included in source checkouts. Supply all four licensed inputs before processing.'
    }
}
Push-Location $root
try {
@'
import array, json, math, pathlib, sys, wave
root = pathlib.Path('assets/audio')
prompts = {
'obstacle': 'One single soft dry muted click for an obstacle navigation cue. Instant onset, very short decay, silence afterwards. Nonverbal isolated mono user interface sound. No voice, no music, no background, no reverb.',
'person': 'Exactly two soft dry rounded electronic tones close together, a distinguishable double tone person navigation cue. Instant onset, short decay, silence afterwards. Nonverbal isolated mono user interface sound. No voice, no music, no background, no reverb.',
'object': 'One bright short dry pluck for an object navigation cue. Instant onset, very short decay, silence afterwards. Nonverbal isolated mono user interface sound. No voice, no music, no background, no reverb.',
'destination': 'One warm soft short dry ping for a destination navigation cue. Instant onset, short decay, silence afterwards. Nonverbal isolated mono user interface sound. No voice, no music, no background, no reverb.'}
report = {'provider': 'ElevenLabs', 'model': 'eleven_text_to_sound_v2', 'requested_duration_seconds': 0.5, 'prompt_influence': 0.7, 'output_format': 'pcm_24000', 'generation_date': '2026-10-12', 'synthetic_fallback': False, 'personal_data_sent': False, 'source_format': 'PROVISIONAL PCM16 LE stereo interleaved 24000Hz; channel layout UNCONFIRMED. Signal analysis is not authoritative and could misclassify mono. Original bytes preserved unchanged.', 'channel_evidence': {'cli': 'elevenlabs text-to-sound-effects convert --schema: output_format describes codec/sample rate/bitrate but no channel count; generation reported audio/pcm without channels', 'official_docs_url': 'https://elevenlabs.io/docs/api-reference/text-to-sound-effects/convert', 'official_docs_result': 'HTTP 403 Forbidden on verification attempt; no authoritative channel evidence obtained', 'consequence': 'If source is mono, provisional pair-averaging interpretation changes timing and pitch. Exact original duration/timbre is not asserted. Replace/reprocess after authoritative confirmation.'}, 'processing': 'Provisionally stereo averaged to mono; trim leading/trailing below -45dB relative peak with 2ms preroll/12ms tail; cap obstacle 160ms, object/destination 280ms, person 400ms; 2ms fade in/8ms fade out; peak normalize -12dBFS. Playback gain is set separately by navigation.ini Volume.', 'limitations': 'Generated placeholders, not perceptually auditioned. Prompt compliance/double-tone distinction requires user listening validation. Stereo pan provides left/right only, not front/back. Source PCM retained for provenance/reprocessing. No synthetic fallback used.', 'cues': {}}
for name, prompt in prompts.items():
    source = array.array('h')
    source.frombytes((root / (name + '.pcm')).read_bytes())
    if sys.byteorder != 'little': source.byteswap()
    assert len(source) == 23040
    mono = [(source[i] + source[i+1]) / 2 for i in range(0, len(source), 2)]
    peak = max(map(abs, mono))
    assert peak > 0
    active = [i for i,v in enumerate(mono) if abs(v) >= peak * 10**(-45/20)]
    start, end = max(0,active[0]-48), min(len(mono),active[-1]+289)
    cap = {'obstacle': 3840, 'person': 9600, 'object': 6720, 'destination': 6720}[name]
    mono = mono[start:min(end, start+cap)]
    for i in range(len(mono)):
        mono[i] *= min(1, i/48, (len(mono)-1-i)/192)
    scale = 32767 * 10**(-12/20) / max(map(abs,mono))
    out = array.array('h', (round(v*scale) for v in mono))
    rms = math.sqrt(sum(v*v for v in out)/len(out))
    measurement = {'prompt': prompt, 'source_bytes': len(source)*2, 'provisional_source_duration_seconds_if_stereo': len(source)/2/24000, 'source_duration_seconds_if_mono': len(source)/24000, 'trimmed_start_seconds': start/24000, 'duration_seconds': len(out)/24000, 'peak_dbfs': 20*math.log10(max(map(abs,out))/32768), 'rms_dbfs': 20*math.log10(rms/32768), 'channels': 1, 'sample_rate': 24000, 'bits': 16}
    report['cues'][name] = measurement
    if sys.byteorder != 'little': out.byteswap()
    with wave.open(str(root/(name+'.wav')), 'wb') as wav:
        wav.setnchannels(1); wav.setsampwidth(2); wav.setframerate(24000); wav.writeframes(out.tobytes())
    print(name, json.dumps(measurement))
(root/'provenance.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8', newline='\n')
'@ | python -
if ($LASTEXITCODE -ne 0) { throw 'Cue processing failed' }
} finally { Pop-Location }
