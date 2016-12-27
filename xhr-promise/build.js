/******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};

/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {

/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId])
/******/ 			return installedModules[moduleId].exports;

/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			exports: {},
/******/ 			id: moduleId,
/******/ 			loaded: false
/******/ 		};

/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);

/******/ 		// Flag the module as loaded
/******/ 		module.loaded = true;

/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}


/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;

/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;

/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "";

/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ function(module, exports) {

	'use strict';

	var _typeof = typeof Symbol === "function" && typeof Symbol.iterator === "symbol" ? function (obj) { return typeof obj; } : function (obj) { return obj && typeof Symbol === "function" && obj.constructor === Symbol && obj !== Symbol.prototype ? "symbol" : typeof obj; };

	(function () {
	  function common(method, url, data, options) {
	    return new Promise(function (resolve, reject) {
	      var xhr = new XMLHttpRequest();
	      options = options || {};
	      xhr.open(method, url);

	      if (/POST|PUT/i.test(method)) {
	        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
	      }

	      if (data && (typeof data === 'undefined' ? 'undefined' : _typeof(data)) === 'object') {
	        var pairs = [];
	        if (data instanceof FormData) {
	          var _iteratorNormalCompletion = true;
	          var _didIteratorError = false;
	          var _iteratorError = undefined;

	          try {
	            for (var _iterator = data.entries()[Symbol.iterator](), _step; !(_iteratorNormalCompletion = (_step = _iterator.next()).done); _iteratorNormalCompletion = true) {
	              var pair = _step.value;

	              pairs.push(encodeURIComponent(pair[0]) + '=' + encodeURIComponent(pair[1]));
	            }
	          } catch (err) {
	            _didIteratorError = true;
	            _iteratorError = err;
	          } finally {
	            try {
	              if (!_iteratorNormalCompletion && _iterator.return) {
	                _iterator.return();
	              }
	            } finally {
	              if (_didIteratorError) {
	                throw _iteratorError;
	              }
	            }
	          }
	        } else {
	          for (var key in data) {
	            pairs.push(encodeURIComponent(key) + '=' + encodeURIComponent(data[key]));
	          }
	        }
	        console.log(pairs);
	        data = pairs.join('&');
	      }

	      if (options.before) {
	        options.before(xhr);
	      }

	      xhr.onreadystatechange = function () {
	        if (xhr.readyState === xhr.DONE) {
	          if (xhr.status === 200) {
	            resolve(xhr);
	          } else {
	            reject(xhr);
	          }
	        }
	      };

	      xhr.send(data);
	    });
	  }

	  var request = {};

	  request.get = function (url, options) {
	    return common('GET', url, undefined, options);
	  };

	  request.post = function (url, data, options) {
	    return common('POST', url, data, options);
	  };

	  request.put = function (url, data, options) {
	    return common('PUT', url, data, options);
	  };

	  request.delete = function (url, options) {
	    return common('DELETE', url, undefined, options);
	  };

	  request.head = function (url, options) {
	    return common('HEAD', url, undefined, options);
	  };

	  window.request = request;
	})();

/***/ }
/******/ ]);