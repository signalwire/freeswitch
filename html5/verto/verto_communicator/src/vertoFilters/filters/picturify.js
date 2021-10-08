(function () {
  'use strict';

  angular
  .module('vertoFilters')
  .filter('picturify', function() {
    var regex = /<a (|target="\s*\S*" )href="(\s*\S*.(png|jpg|svg|gif|webp|bmp))">\s*\S*<\/a>/i;
    var regex64 = /data:image\/(\s*\S*);base64,((?:[A-Za-z0-9+\/]{4})*(?:[A-Za-z0-9+\/]{2}==|[A-Za-z0-9+\/]{3}=))/g;

    return function (text, width, n) {
      var i = 0;
      width = width || 150; //default width
      if(regex64.test(text)) {
        text = text.replace(regex64, '<a href="$1" target="_blank"><img class="chat-img" width="'+ width +'" src="data:image/png;base64,$2"/></a>')
      }

      do {
        text = text.replace(regex, '<a $1href="$2"><img class="chat-img" width="'+ width +'" src="$2"/></a>');
      } while((!n || (n && i++ < n)) && regex.test(text));

      return text;
    }

  });

})();
