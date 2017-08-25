import Terminal from 'xterm';
import 'xterm/src/xterm.css';
import io from 'socket.io-client';

Terminal.loadAddon('fit');

const socket = io(window.location.href);

const term = new Terminal({
  cols: 80,
  rows: 24,
});
term.open(document.getElementById('#terminal'));
// term.on('resize', size => {
//   socket.emit('resize', [size.cols, size.rows]);
// })

term.on('data', data => socket.emit('input', data));

socket.on('output', arrayBuffer => {
  term.write(String.fromCharCode.apply(null, new Uint8Array(arrayBuffer)));
});

// window.addEventListener('resize', () => {
//   term.fit()
// });
// // term.fit()
