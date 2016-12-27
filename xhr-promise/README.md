### Description

Simple HTTP client with Promise and XMLHttpRequest.

#### Useage
```html
<script src="//cdn.bootcss.com/babel-polyfill/6.20.0/polyfill.min.js"></script>
<script src="build.js"></script>
<script>
  function Foo () {
    request.get('/api/v1/blabla').then(function (xhr) {
      console.log('response text: ' + xhr.responseText);
    }, function (xhr) {
      console.log('response error: ' + xhr.statusText);
    })
  }
</script>
```
