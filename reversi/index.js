var express = require('express')
var app = express();
var server = require('http').Server(app);
var io = require('socket.io')(server);

server.listen(23333);

app.use('/', express.static('./dist'));

let waitingClient = null;

const EMPTY = 0;
const WHITE = 1;
const BLACK = 2;

class Game {
  constructor(firstPlayer, secondPlayer) {
    this.firstPlayer = firstPlayer;
    this.secondPlayer = secondPlayer;
    this.board = new Array(8).fill(0).map(row => new Array(8).fill(EMPTY));
    this.nextValidPlayer = BLACK;

    this.board[3][3] = this.board[4][4] = WHITE;
    this.board[3][4] = this.board[4][3] = BLACK;

    firstPlayer.emit('message', 'BLACK');
    secondPlayer.emit('message', 'WHITE');

    firstPlayer.emit('next_player', 'BLACK');
    secondPlayer.emit('next_player', 'BLACK');

    firstPlayer.emit('board', this.board);
    secondPlayer.emit('board', this.board);

    firstPlayer.on('op', this.createInputHandler(BLACK));
    secondPlayer.on('op', this.createInputHandler(WHITE));
  }

  createInputHandler(playerColor) {
    return function (payload) {
      console.log({payload, playerColor, nextValidPlayer: this.nextValidPlayer })

      if (this.nextValidPlayer !== playerColor) {
        return;
      }

      let [x, y] = payload;
      let changeCount = 0;

      if (this.board[x][y] !== EMPTY) {
        return;
      }

      let vectors = [
        [1, 0], [1, 1], [0, 1], [-1, 1],
        [-1, 0], [-1, -1], [0, -1], [1, -1]
      ];

      vectors.forEach(vector => {
        let cursor_x = x;
        let cursor_y = y;
        let valid = true;

        for (let i = 0;; i++) {
          cursor_x += vector[0];
          cursor_y += vector[1];

          if (cursor_x < 0 || cursor_y < 0 || cursor_x >= 8 || cursor_y >= 8) {
            valid = false;
            break;
          }

          if (this.board[cursor_x][cursor_y] === playerColor) {
            break;
          }

          if (this.board[cursor_x][cursor_y] === EMPTY) {
            valid = false;
            break;
          }
        }

        if (valid) {
          const end_x = cursor_x;
          const end_y = cursor_y;
          
          cursor_x = x;
          cursor_y = y;
  
          for (let i = 0;; i++) {
            cursor_x += vector[0];
            cursor_y += vector[1];

            if (cursor_x === end_x && cursor_y === end_y) {
              break;
            }

            this.board[cursor_x][cursor_y] = playerColor;
            changeCount++;
          }
        }
      })

      if (changeCount !== 0) {
        this.board[x][y] = playerColor;

        if (playerColor === WHITE) {
          this.nextValidPlayer = BLACK;
          this.firstPlayer.emit('next_player', 'BLACK');
          this.secondPlayer.emit('next_player', 'BLACK');
        }
        if (playerColor === BLACK) {
          this.nextValidPlayer = WHITE;
          this.firstPlayer.emit('next_player', 'WHITE');
          this.secondPlayer.emit('next_player', 'WHITE');
        }

        this.firstPlayer.emit('board', this.board);
        this.secondPlayer.emit('board', this.board);

      }
    }.bind(this);
  }
}

io.on('connection', (socket) => {
  if (waitingClient === null) {
    waitingClient = socket;
  } else {
    new Game(waitingClient, socket);
    waitingClient = null;
  }
});

