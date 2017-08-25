const express = require('express');
const site = express();
const http = require('http').Server(site);
const io = require('socket.io')(http);
const net = require('net');
const pty = require('node-pty');

site.use('/', express.static('.'));

io.on('connection', function (socket) {
  let ptyProcess = pty.spawn('bash', ['--login'], {
    name: 'xterm-color',
    cols: 80,
    rows: 24,
    cwd: process.env.HOME,
    env: process.env
  });
  ptyProcess.on('data', data => socket.emit('output', data));
  socket.on('input', data => ptyProcess.write(data));
  socket.on('resize', size => {
    console.log(size);
    ptyProcess.resize(size[0], size[1])
  });
});

http.listen(8123);
