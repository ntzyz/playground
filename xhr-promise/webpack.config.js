module.exports = {
  entry: './src.js',
  output: {
    filename: 'build.js',
    path: '.'
  },
  module: {
    loaders: [
      {
        test: /\.js$/,
        exclude: /(node_modules|bower_components)/,
        loader: 'babel-loader?presets[]=es2015'
      }
    ]
  }
}