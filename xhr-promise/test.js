'use strict';

const express = require('express');
const bodyParser = require('body-parser');

let site = express();
site.use(bodyParser.urlencoded({extended: true}));
site.use(bodyParser.json());
site.use('/root', express.static('.'));

let html = `<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8"/>
    <style>
      button { display: block; margin: 5px; }
    </style>
  </head>
  <body>
    <button onclick="testHttpGet()">HTTP GET test</button>
    <button onclick="testHttpPostObject()">HTTP POST object test</button>
    <button onclick="testHttpPostFormData()">HTTP POST FormData test</button>
    <button onclick="testHttpPut()">HTTP PUT test</button>
    <button onclick="testHttpDelete()">HTTP DELETE test</button>
    <button onclick="testHttpHead()">HTTP HEAD test</button>
    <div>Log:</div>
    <pre id="log"></pre>
    <script src="/root/src.js"></script>
    <script>
      var log = document.getElementById('log');

      function testHttpGet () {
        request.get('/http')
        .then(function (xhr) {
          log.innerText += 'Test complete, response text: ' + xhr.responseText + '\\n';
        }, function (xhr) {
          log.innerText += 'Test complete, response error: ' + xhr.statusText + '\\n';
        })
      }

      function testHttpPostObject () {
        request.post('/http', {
          part1: 'lazy dog',
          part2: 'quick brown fox'
        })
        .then(function (xhr) {
          log.innerText += 'Test complete, response text: ' + xhr.responseText + '\\n';
        }, function (xhr) {
          log.innerText += 'Test complete, response error: ' + xhr.statusText + '\\n';
        })
      }

      function testHttpPostFormData () {
        var formData = new FormData();

        formData.append('part1', 'lazy dog');
        formData.append('part2', 'quick brown fox');

        request.post('/http', formData)
        .then(function (xhr) {
          log.innerText += 'Test complete, response text: ' + xhr.responseText + '\\n';
        }, function (xhr) {
          log.innerText += 'Test complete, response error: ' + xhr.statusText + '\\n';
        })
      }

      function testHttpPut () {
        request.put('/http', {
          part1: 'lazy dog',
          part2: 'quick brown fox'
        })
        .then(function (xhr) {
          log.innerText += 'Test complete, response text: ' + xhr.responseText + '\\n';
        }, function (xhr) {
          log.innerText += 'Test complete, response error: ' + xhr.statusText + '\\n';
        })
      }

      function testHttpDelete () {
        request.delete('/http')
        .then(function (xhr) {
          log.innerText += 'Test complete, response text: ' + xhr.responseText + '\\n';
        }, function (xhr) {
          log.innerText += 'Test complete, response error: ' + xhr.statusText + '\\n';
        })
      }

      function testHttpHead () {
        request.head('/http')
        .then(function (xhr) {
          log.innerText += 'Test complete, response length: ' + xhr.getResponseHeader("Content-Length") + '\\n';
        }, function (xhr) {
          log.innerText += 'Test complete, response error: ' + xhr.statusText + '\\n';
        })
      }
    </script>
  </body>
</html>
`;

site.get('/', (req, res) => {
  res.send(html);
})

site.get('/http', (req, res) => {
  res.send('The quick brown fox jumps over the lazy dog.');
})

site.post('/http', (req, res) => {
  res.send(`The ${req.body.part1} jumps over the ${req.body.part2}.`);
})

site.put('/http', (req, res) => {
  res.send(`The ${req.body.part1} jumps over the ${req.body.part2}.`);
});

site.delete('/http', (req, res) => {
  res.send(JSON.stringify({status: 'ok'}));
});

site.listen(8080);
