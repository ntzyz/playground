'use strict';

var _createClass = function () { function defineProperties(target, props) { for (var i = 0; i < props.length; i++) { var descriptor = props[i]; descriptor.enumerable = descriptor.enumerable || false; descriptor.configurable = true; if ("value" in descriptor) descriptor.writable = true; Object.defineProperty(target, descriptor.key, descriptor); } } return function (Constructor, protoProps, staticProps) { if (protoProps) defineProperties(Constructor.prototype, protoProps); if (staticProps) defineProperties(Constructor, staticProps); return Constructor; }; }();

function _classCallCheck(instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError("Cannot call a class as a function"); } }

var start = document.querySelector('button[start]');
var stop = document.querySelector('button[stop]');
var frameSpan = document.querySelector('div[frame]');
var status = document.querySelector('span[status]');
var canvas = document.querySelector('canvas');
var select = document.querySelector('select[song]');
var glCtx = canvas.getContext('2d');

function $get(url) {
  return new Promise(function (resolve, reject) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url);
    xhr.responseType = 'arraybuffer';
    xhr.addEventListener('progress', function (e) {
      status.innerText = 'Loading media ' + (100 * e.loaded / e.total).toFixed(3) + '% ...';
    });
    xhr.onreadystatechange = function () {
      switch (xhr.readyState) {
        case xhr.DONE:
          resolve(xhr.response);
          break;
        default:
          break;
      }
    };
    xhr.send();
  });
}

var Wav = function () {
  function Wav(file) {
    _classCallCheck(this, Wav);

    var rawFile = file;
    file = new Uint8Array(rawFile);

    // Reference for RIFF chunk and FMT chunk
    var RIFF = {
      RIFF: { begin: 0, length: 4, type: 'CHAR' },
      ChunkSize: { begin: 4, length: 4, type: 'INTEGER' },
      WAVE: { begin: 8, length: 4, type: 'CHAR' }
    };
    var FMT = {
      fmt: { begin: 12, length: 4, type: 'CHAR' },
      Subchunk1Size: { begin: 16, length: 2, type: 'INTEGER' },
      AudioFormat: { begin: 20, length: 2, type: 'INTEGER' },
      NumOfChan: { begin: 22, length: 2, type: 'INTEGER' },
      SamplesPerSec: { begin: 24, length: 4, type: 'INTEGER' },
      bytesPerSec: { begin: 28, length: 4, type: 'INTEGER' },
      blockAlign: { begin: 32, length: 2, type: 'INTEGER' },
      bitsPerSample: { begin: 34, length: 2, type: 'INTEGER' }
    };

    file.subArray = function (begin, length) {
      return file.slice(begin, begin + length);
    };

    var readChar = function readChar(begin, length) {
      return Array.from(file.subArray(begin, length)).map(function (ch) {
        return String.fromCharCode(ch);
      }).join('');
    };

    var readInt = function readInt(begin, length) {
      return Array.from(file.subArray(begin, length)).reverse().reduce(function (a, b) {
        return a * 256 + b;
      });
    };

    var readChunk = function readChunk(ref, save) {
      for (var item in ref) {
        switch (ref[item].type) {
          case 'CHAR':
            save[item] = readChar(ref[item].begin, ref[item].length);
            break;
          case 'INTEGER':
            save[item] = readInt(ref[item].begin, ref[item].length);
            break;
        }
      }
    };

    // Read the RIFF chunk
    readChunk(RIFF, this);

    // Keep reading data until finding the 'data' chunk
    for (var offset = 12;;) {
      var chunkName = readChar(offset, 4);offset += 4;
      var chunkSize = readInt(offset, 4);offset += 4;
      this[chunkName] = {};
      this[chunkName].size = chunkSize;
      if (chunkName === 'fmt ') {
        // Read the RIFF chunk
        readChunk(FMT, this[chunkName]);
      } else {
        if (chunkName == 'data') {
          var type = 'Int' + this['fmt '].bitsPerSample + 'Array';
          this[chunkName] = new window[type](rawFile.slice(offset, chunkSize));
          break;
        }
        // Read as raw
        this[chunkName]._rawData = file.subArray(offset, chunkSize);
      }
      offset += chunkSize;
    }
  }

  _createClass(Wav, [{
    key: 'step',
    value: regeneratorRuntime.mark(function step() {
      var ctx, framesCount, audioBuffer, channelBuffering, i, last500framse, currentFrame;
      return regeneratorRuntime.wrap(function step$(_context) {
        while (1) {
          switch (_context.prev = _context.next) {
            case 0:
              ctx = new AudioContext();
              framesCount = wav.ChunkSize / (wav['fmt '].bitsPerSample / 8);
              audioBuffer = ctx.createBuffer(wav['fmt '].NumOfChan, framesCount / 2, wav['fmt '].SamplesPerSec);
              channelBuffering = [];

              this.channelBuffering = channelBuffering;
              for (i = 0; i < wav['fmt '].NumOfChan; ++i) {
                channelBuffering[i] = audioBuffer.getChannelData(i);
              }

              this.source = ctx.createBufferSource();
              this.source.loop = false;
              this.source.buffer = audioBuffer;
              this.source.connect(ctx.destination);
              last500framse = this.last500framse = new Array(500).fill(0);

              this.sampleingInterval = 16;

              currentFrame = 0;

            case 13:
              if (!(currentFrame < framesCount)) {
                _context.next = 21;
                break;
              }

              channelBuffering[currentFrame % 2][Math.ceil(currentFrame / 2)] = Number(wav.data[currentFrame]) / (1 << wav['fmt '].bitsPerSample - 1);

              if (currentFrame % (this.sampleingInterval * 2) === 1) {
                last500framse.shift();
                last500framse.push((channelBuffering[1][Math.ceil(currentFrame / 2)] + channelBuffering[0][Math.ceil(currentFrame / 2)]) / 2);
              }
              _context.next = 18;
              return currentFrame;

            case 18:
              ++currentFrame;
              _context.next = 13;
              break;

            case 21:
            case 'end':
              return _context.stop();
          }
        }
      }, step, this);
    })
  }]);

  return Wav;
}();

function main() {
  $get(select.value).then(function (file) {
    status.innerText = 'Loading media ... ';
    start.style.display = 'none';
    window.wav = new Wav(file);
    status.innerText = 'Playing ... ';
    var begin = new Date();
    var iterator = wav.step();
    window.intervalId = setInterval(function () {
      var current = new Date();
      var diff = current.getTime() - begin.getTime();
      var last = void 0;
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
      frameSpan.innerText = 'Current Frame Offset: ' + last + '\n' + ('Time Offset(Frame): ' + last / wav['fmt '].SamplesPerSec / wav['fmt '].NumOfChan + '\n') + ('Time Offset(Clock): ' + diff / 1000);
      // Draw the visualizer.
      var width = canvas.width;
      var height = canvas.height;
      glCtx.clearRect(0, 0, width, height);
      glCtx.beginPath();
      glCtx.moveTo(0, canvas.height / 2);
      for (var i = 0; i < 500; ++i) {
        glCtx.lineTo(i * canvas.width / 500, canvas.height / 2 * (wav.last500framse[i] + 1));
      }
      glCtx.stroke();
    }, 10);
    console.log(wav);
    setTimeout(function () {
      wav.source.start();
    }, 10);
  });
}

start.onclick = main;
