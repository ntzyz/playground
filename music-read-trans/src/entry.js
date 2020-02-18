import Vex from 'vexflow';

const VF = Vex.Flow;

const div = document.querySelector('div#app');
const renderer = new VF.Renderer(div, VF.Renderer.Backends.SVG);
const button = document.querySelector('button');
const result = document.querySelector('pre');

document.querySelector('input').value = '';

renderer.resize(500, 300);

const availableKeys = [
  'c/3',
  'd/3',
  'e/3',
  'f/3',
  'g/3',
  'a/3',
  'b/3',
  'c/4',
  'd/4',
  'e/4',
  'f/4',
  'g/4',
  'a/4',
  'b/4',
  'c/5',
  'd/5',
  'e/5',
  'f/5',
  'g/5',
];

function getRandKey() {
  const ret = availableKeys[Math.floor(Math.random() * availableKeys.length)];
  return ret;
}

// And get a drawing context:
const context = renderer.getContext();

let notesTreble = [
  new VF.StaveNote({ clef: "treble", keys: [getRandKey()], duration: "q" }),
  new VF.StaveNote({ clef: "treble", keys: [getRandKey()], duration: "q" }),
  new VF.StaveNote({ clef: "treble", keys: [getRandKey()], duration: "q" }),
  new VF.StaveNote({ clef: "treble", keys: [getRandKey()], duration: "q" }),
  new Vex.Flow.BarNote(),
  new VF.StaveNote({ clef: "treble", keys: [getRandKey()], duration: "q" }),
  new VF.StaveNote({ clef: "treble", keys: [getRandKey()], duration: "q" }),
  new VF.StaveNote({ clef: "treble", keys: [getRandKey()], duration: "q" }),
  new VF.StaveNote({ clef: "treble", keys: [getRandKey()], duration: "q" }),
  new Vex.Flow.BarNote(),
  new VF.StaveNote({ clef: "treble", keys: [getRandKey()], duration: "q" }),
  new VF.StaveNote({ clef: "treble", keys: [getRandKey()], duration: "q" }),
  new VF.StaveNote({ clef: "treble", keys: [getRandKey()], duration: "q" }),
  new VF.StaveNote({ clef: "treble", keys: [getRandKey()], duration: "q" }),
  new Vex.Flow.BarNote(),
  new VF.StaveNote({ clef: "treble", keys: [getRandKey()], duration: "q" }),
  new VF.StaveNote({ clef: "treble", keys: [getRandKey()], duration: "q" }),
  new VF.StaveNote({ clef: "treble", keys: [getRandKey()], duration: "q" }),
  new VF.StaveNote({ clef: "treble", keys: [getRandKey()], duration: "q" }),
  new Vex.Flow.BarNote(),
];

let notesBass = [
  undefined,
  undefined,
  undefined,
  undefined,
  new Vex.Flow.BarNote(),
  undefined,
  undefined,
  undefined,
  undefined,
  new Vex.Flow.BarNote(),
  undefined,
  undefined,
  undefined,
  undefined,
  new Vex.Flow.BarNote(),
  undefined,
  undefined,
  undefined,
  undefined,
  new Vex.Flow.BarNote(),
];

let answers = notesTreble.map(el => {
  if (!el.keys) {
    return null;
  }
  const map = {
    c: 'do',
    d: 're',
    e: 'mi',
    f: 'fa',
    g: 'so',
    a: 'la',
    b: 'xi',
  }
  return map[el.keys[0][0]];
}).filter(el => el);

console.log(answers)

for (let i = 0; i < notesTreble.length; i++) {
  if (i % 5 == 4) {
    continue;
  }

  const key = notesTreble[i].keys[0];

  if (key[2] == '3') {
    notesBass[i] = new VF.StaveNote({ clef: "bass", keys: [key], duration: "q" }),
    notesTreble[i] = undefined;
  }
}

for (let i = 0; i < 4; i++) {
  for (let j = 0; j < 4;) {
    if (notesTreble[i * 5 + j] === undefined) {
      let length = 0;

      do {
        length++;
      } while (notesTreble[i * 5 + j + length] === undefined);

      switch (length) {
        case 1: notesTreble[i * 5 + j] = new VF.StaveNote({ clef: "treble", keys: ['g/4'], duration: "qr" }); break;
        case 2: notesTreble[i * 5 + j] = new VF.StaveNote({ clef: "treble", keys: ['g/4'], duration: "hr" }); break;
        case 3: notesTreble[i * 5 + j] = new VF.StaveNote({ clef: "treble", keys: ['g/4'], duration: "hdr" }).addDot(0); break;
        case 4: notesTreble[i * 5 + j] = new VF.StaveNote({ clef: "treble", keys: ['g/4'], duration: "1r" }); break;
      }

      j += length;
    } else {
      j++;
    }
  }

  for (let j = 0; j < 4;) {
    if (notesBass[i * 5 + j] === undefined) {
      let length = 0;

      do {
        length++;
      } while (notesBass[i * 5 + j + length] === undefined);

      switch (length) {
        case 1: notesBass[i * 5 + j] = new VF.StaveNote({ clef: "bass", keys: ['f/3'], duration: "qr" }); break;
        case 2: notesBass[i * 5 + j] = new VF.StaveNote({ clef: "bass", keys: ['f/3'], duration: "hr" }); break;
        case 3: notesBass[i * 5 + j] = new VF.StaveNote({ clef: "bass", keys: ['f/3'], duration: "hdr" }).addDot(0); break;
        case 4: notesBass[i * 5 + j] = new VF.StaveNote({ clef: "bass", keys: ['f/3'], duration: "1r" }); break;
      }

      j += length;
    } else {
      j++;
    }
  }
}

notesTreble = notesTreble.filter(el => el);
notesBass = notesBass.filter(el => el);

console.log({
  notesUp: notesTreble,
  notesDo: notesBass
});

const staffTreble = new VF.Stave(30, 40, 600);
staffTreble.addClef("treble").addTimeSignature("4/4");
staffTreble.setContext(context).draw();

const voiceTreble = new VF.Voice({ num_beats: 16, beat_value: 4 });
voiceTreble.addTickables(notesTreble);

const staffBass = new VF.Stave(30, 140, 600);
staffBass.addClef("bass").addTimeSignature("4/4");
staffBass.setContext(context).draw();

const voiceBass = new VF.Voice({ num_beats: 16, beat_value: 4 });
voiceBass.addTickables(notesBass);

const startX = Math.max(staffTreble.getNoteStartX(), staffBass.getNoteStartX());
staffTreble.setNoteStartX(startX);
staffBass.setNoteStartX(startX);

const formatter = new Vex.Flow.Formatter();
formatter.joinVoices([voiceTreble, voiceBass]);
formatter.format([voiceTreble, voiceBass]);

new VF.StaveConnector(staffTreble, staffBass).setType(VF.StaveConnector.type.BRACE).setContext(context).draw();

new VF.StaveConnector(staffTreble, staffBass).setType(VF.StaveConnector.type.SINGLE_LEFT).setContext(context).draw();

new VF.StaveConnector(staffTreble, staffBass).setType(VF.StaveConnector.type.SINGLE_RIGHT).setContext(context).draw();

voiceTreble.draw(context, staffTreble);
voiceBass.draw(context, staffBass);

button.addEventListener('click', () => {
  result.innerHTML = `标准答案：${answers.join(' ')}\n你的答案：${document.querySelector('input').value}`;
});