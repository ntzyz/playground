const express = require('express');
const site = express();
const http = require('http').Server(site);
const io = require('socket.io')(http);
const net = require('net');
const pty = require('node-pty');

site.use('/', express.static('.'));

io.on('connection', function (socket) {
  let ptyProcess = pty.spawn('zsh', ['--login'], {
    name: 'xterm-color',
    cols: 80,
    rows: 24,
    cwd: process.env.HOME,
    env: process.env
  });
  ptyProcess.on('data', data => socket.emit('output', Buffer.from(data)));
  socket.on('input', data => ptyProcess.write(data));
  socket.on('resize', size => ptyProcess.resize(size.cols, size.rows));
});

http.listen(8080);