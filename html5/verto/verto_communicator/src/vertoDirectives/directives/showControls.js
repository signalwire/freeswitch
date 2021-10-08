(function () {
  'use strict';

  angular
  .module('vertoDirectives')
  .directive('showControls',
  function(Fullscreen) {
    var link = function(scope, element, attrs) {
      var i = null;
      jQuery('.video-footer').fadeIn('slow');
      jQuery('.video-hover-buttons').fadeIn('slow');
      element.parent().bind('mousemove', function() {
        if (Fullscreen.isEnabled()) {
          clearTimeout(i);
          jQuery('.video-footer').fadeIn('slow');
          jQuery('.video-hover-buttons').fadeIn(500);
          i = setTimeout(function() {
            if (Fullscreen.isEnabled()) {
              jQuery('.video-footer').fadeOut('slow');
              jQuery('.video-hover-buttons').fadeOut(500);
            }
          }, 3000);
        }
      });
      element.parent().bind('mouseleave', function() {
        jQuery('.video-footer').fadeIn();
        jQuery('.video-hover-buttons').fadeIn();
      });
    }


    return {
      link: link
    };
  });

})();