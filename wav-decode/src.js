'use strict';

let start = document.querySelector('button[start]');
let stop = document.querySelector('button[stop]');
let frameSpan = document.querySelector('div[frame]');
let status = document.querySelector('span[status]');
let canvas = document.querySelector('canvas');
let select = document.querySelector('select[song]');
let glCtx = canvas.getContext('2d');

function $get(url) {
  return new Promise((resolve, reject) => {
    let xhr = new XMLHttpRequest();
    xhr.open('GET', url);
    xhr.responseType = 'arraybuffer';
    xhr.addEventListener('progress', (e) => {
      status.innerText = `Loading media ${(100 * e.loaded / e.total).toFixed(3)}% ...`;
    });
    xhr.onreadystatechange = () => {
      switch(xhr.readyState) {
      case xhr.DONE:
        resolve(xhr.response)
        break;
      default:
        break;
      }
    };
    xhr.send();
  });
}

class Wav {
  constructor(file) {
    let rawFile = file;
    file = new Uint8Array(rawFile);

    // Reference for RIFF chunk and FMT chunk
    let RIFF = {
      RIFF:           { begin:    0, length:    4, type: 'CHAR'},
      ChunkSize:      { begin:    4, length:    4, type: 'INTEGER'},
      WAVE:           { begin:    8, length:    4, type: 'CHAR'},
    }
    let FMT = {
      fmt:            { begin:   12, length:    4, type: 'CHAR'},
      Subchunk1Size:  { begin:   16, length:    2, type: 'INTEGER'},
      AudioFormat:    { begin:   20, length:    2, type: 'INTEGER'},
      NumOfChan:      { begin:   22, length:    2, type: 'INTEGER'},
      SamplesPerSec:  { begin:   24, length:    4, type: 'INTEGER'},
      bytesPerSec:    { begin:   28, length:    4, type: 'INTEGER'},
      blockAlign:     { begin:   32, length:    2, type: 'INTEGER'},
      bitsPerSample:  { begin:   34, length:    2, type: 'INTEGER'},
    }
    
    file.subArray = (begin, length) => {
      return file.slice(begin, begin + length);
    };

    let readChar = (begin, length) => Array
      .from(file
        .subArray(begin, length))
      .map(ch => String.fromCharCode(ch))
      .join('');

    let readInt = (begin, length) => Array
      .from(file
        .subArray(begin, length))
      .reverse()
      .reduce((a, b) => a * 256 + b);

    let readChunk = (ref, save) => {
      for (let item in ref) {
        switch(ref[item].type) {
          case 'CHAR':
            save[item] = readChar(ref[item].begin, ref[item].length);
            break;
          case 'INTEGER':
            save[item] = readInt(ref[item].begin, ref[item].length);
            break;
        }
      }
    }

    // Read the RIFF chunk
    readChunk(RIFF, this);

    // Keep reading data until finding the 'data' chunk
    for (let offset = 12;;) {
      let chunkName = readChar(offset, 4); offset += 4;
      let chunkSize = readInt(offset, 4); offset += 4;
      this[chunkName] = {};
      this[chunkName].size = chunkSize;
      if (chunkName === 'fmt ') {
        // Read the RIFF chunk
        readChunk(FMT, this[chunkName]);
      }
      else {
        if (chunkName == 'data') {
          let type = `Int${this['fmt '].bitsPerSample}Array`;
          this[chunkName] = new window[type](rawFile.slice(offset, chunkSize))
          break;
        }
        // Read as raw
        this[chunkName]._rawData = file.subArray(offset, chunkSize);
      }
      offset += chunkSize;
    }
  }
  *step() {
    let ctx = new AudioContext();
    let framesCount = wav.ChunkSize / (wav['fmt '].bitsPerSample / 8);

    let audioBuffer = ctx.createBuffer(
      wav['fmt '].NumOfChan, 
      framesCount / 2, 
      wav['fmt '].SamplesPerSec
    );

    let channelBuffering = [];
    this.channelBuffering = channelBuffering;
    for (let i = 0; i < wav['fmt '].NumOfChan; ++i) {
      channelBuffering[i] = audioBuffer.getChannelData(i)
    }

    this.source = ctx.createBufferSource();
    this.source.loop = false;
    this.source.buffer = audioBuffer;
    this.source.connect(ctx.destination);
    let last500framse = this.last500framse = new Array(500).fill(0);
    this.sampleingInterval = 16;
    
    for (let currentFrame = 0; currentFrame < framesCount; ++currentFrame) {
      channelBuffering[currentFrame % 2][Math.ceil(currentFrame / 2)]
          = Number(wav.data[currentFrame]) / (1 << (wav['fmt '].bitsPerSample - 1));

      if (currentFrame % (this.sampleingInterval * 2) === 1) {
        last500framse.shift();
        last500framse.push((channelBuffering[1][Math.ceil(currentFrame / 2)] + channelBuffering[0][Math.ceil(currentFrame / 2)]) / 2);
      }
      yield currentFrame;
    }
  }
}

function main() {
  $get(select.value).then((file) => {
    status.innerText = 'Loading media ... ';
    start.style.display = 'none';
    window.wav = new Wav(file);
    status.innerText = 'Playing ... ';
    let begin = new Date();
    let iterator = wav.step();
    window.intervalId = setInterval(() => {
      let current = new Date();
      let diff = current.getTime() - begin.getTime();
      let last;
      for (;;) {
        last = iterator.next().value;
        if (typeof last === 'undefined') {
          clearInterval(window.intervalId);
          start.style.display = '';
          status.innerText = 'Ready ... ';
          console.info('end');
          break;
        }
        if (last / wav['fmt '].SamplesPerSec / wav['fmt '].NumOfChan > diff / 1000) break;
      }
      frameSpan.innerText 
        = `Current Frame Offset: ${last}\n` 
        + `Time Offset(Frame): ${last / wav['fmt '].SamplesPerSec / wav['fmt '].NumOfChan}\n`
        + `Time Offset(Clock): ${diff / 1000}`;
      // Draw the visualizer.
      let width = canvas.width;
      let height = canvas.height;
      glCtx.clearRect(0, 0, width, height);
      glCtx.beginPath();
      glCtx.moveTo(0, canvas.height / 2);
      for (let i = 0; i < 500; ++i) {
        glCtx.lineTo(i * canvas.width / 500, canvas.height / 2 * (wav.last500framse[i] + 1) );
      }
      glCtx.stroke();
    }, 10);
    console.log(wav);
    setTimeout(() => {wav.source.start();}, 10)
  });
}

start.onclick = main;
