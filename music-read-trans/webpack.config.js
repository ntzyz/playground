const path = require('path');

module.exports = {
  entry: "./src/entry.js",
  output: {
    path: path.join(__dirname, 'dist'),
    filename: "bundle.js"
  },
  devServer: {
    contentBase: path.join(__dirname, '.'),
    index: 'index.html',
    compress: true,
    port: 9000,
    host: '0.0.0.0',
  }
};