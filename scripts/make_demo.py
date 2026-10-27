"""Generate an original eight-bar score and matching synthetic WAV, with no downloads."""

from array import array
import math
from pathlib import Path
import sys
import wave


def generate(directory):
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    notes = [("C", 60), ("D", 62), ("E", 64), ("G", 67)]
    measures = []
    samples = array("h")
    rate = 44100
    for bar in range(8):
        content = ""
        if bar == 0:
            content = '''<attributes><divisions>1</divisions><key><fifths>0</fifths></key>
<time><beats>4</beats><beat-type>4</beat-type></time><clef><sign>G</sign><line>2</line></clef></attributes>
<direction placement="above"><direction-type><metronome><beat-unit>quarter</beat-unit><per-minute>120</per-minute></metronome></direction-type><sound tempo="120"/></direction>'''
        for beat in range(4):
            step, midi = notes[(beat + bar) % len(notes)]
            content += f'<note><pitch><step>{step}</step><octave>4</octave></pitch><duration>1</duration><type>quarter</type></note>'
            frequency = 440 * 2 ** ((midi - 69) / 12)
            for i in range(rate // 2):
                t = i / rate
                envelope = min(1, t / 0.005) * max(0, min(1, (0.4 - t) / 0.03))
                samples.append(round(5000 * envelope * math.sin(2 * math.pi * frequency * t)))
        measures.append(f'<measure number="{bar + 1}">{content}</measure>')
    xml = '''<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE score-partwise PUBLIC "-//Recordare//DTD MusicXML 4.0 Partwise//EN" "http://www.musicxml.org/dtds/partwise.dtd">
<score-partwise version="4.0"><work><work-title>Backing Track Sync Demo</work-title></work>
<part-list><score-part id="P1"><part-name>Piano</part-name><part-abbreviation>Pno.</part-abbreviation>
<score-instrument id="I1"><instrument-name>Piano</instrument-name></score-instrument>
<midi-instrument id="I1"><midi-channel>1</midi-channel><midi-program>1</midi-program></midi-instrument>
</score-part></part-list><part id="P1">''' + "".join(measures) + "</part></score-partwise>"
    score = directory / "Backing Track Sync Demo.musicxml"
    score.write_text(xml, encoding="utf-8")
    if sys.byteorder != "little":
        samples.byteswap()
    audio = directory / "Backing Track Sync Demo.wav"
    with wave.open(str(audio), "wb") as writer:
        writer.setnchannels(1)
        writer.setsampwidth(2)
        writer.setframerate(rate)
        writer.writeframes(samples.tobytes())
    return score, audio


if __name__ == "__main__":
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent / "demo"
    for path in generate(output):
        print(path)
