'use strict';

/* Directives */

var vertoDirectives = angular.module('vertoDirectives', []);

/**
 * To RTC work properly we need to give a <video> tag as soon as possible
 * because it needs to attach the video and audio stream to the tag.
 *
 * This directive is responsible for moving the video tag from the body to
 * the right place when a call start and move back to the body when the
 * call ends. It also hides and display the tag when its convenient.
 */
vertoDirectives.directive('videoTag',
  function() {
    function link(scope, element, attrs) {
      // Moving the video tag to the new place inside the incall page.
      console.log('Moving the video to element.');
      jQuery('video').removeClass('hide').appendTo(element);
      jQuery('video').css('display','block');
      scope.callActive();

      element.on('$destroy', function() {
        // Move the video back to the body.
        console.log('Moving the video back to body.');
        jQuery('video').addClass('hide').appendTo(jQuery('body'));
      });
    }

    return {
      link: link
    }
  });

vertoDirectives.directive('userStatus',
  function() {
    var link = function(scope, element, attrs) {
      scope.$watch('condition', function(condition) {
        element.removeClass('connected');
        element.removeClass('disconnected');
        element.removeClass('connecting');

        element.addClass(condition);

      });
    }

    return {
      scope: {
        'condition': '='
      },
      link: link
    };

  });

vertoDirectives.directive('showControls',
  function(Fullscreen) {
    var link = function(scope, element, attrs) {
      var i = null;
      jQuery('.video-footer').fadeIn('slow');
      jQuery('.video-hover-buttons').fadeIn('slow');
      element.parent().bind('mousemove', function() {
        if(Fullscreen.isEnabled()) {
          clearTimeout(i);
          jQuery('.video-footer').fadeIn('slow');
          jQuery('.video-hover-buttons').fadeIn(500);
          i = setTimeout(function () {
            if(Fullscreen.isEnabled()) {
              jQuery('.video-footer').fadeOut('slow');
              jQuery('.video-hover-buttons').fadeOut(500);
            }
          }, 3000);
        }
      });
      element.parent().bind('mouseleave', function () {
        jQuery('.video-footer').fadeIn();
        jQuery('.video-hover-buttons').fadeIn();
      });
    }


    return {
      link: link
    };
});
