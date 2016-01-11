/**
 * To RTC work properly we need to give a <video> tag as soon as possible
 * because it needs to attach the video and audio stream to the tag.
 *
 * This directive is responsible for moving the video tag from the body to
 * the right place when a call start and move back to the body when the
 * call ends. It also hides and display the tag when its convenient.
 */
(function () {
  'use strict';

  angular
  .module('vertoDirectives')
  .directive('videoTag',
  function() {
    function link(scope, element, attrs) {
      // Moving the video tag to the new place inside the incall page.
      console.log('Moving the video to element.');
      jQuery('video').removeClass('hide').appendTo(element);
      jQuery('video').css('display', 'block');
      scope.callActive("", {useVideo: true});

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

})();
