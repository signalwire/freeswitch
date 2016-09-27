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
      var videoElem = jQuery('#webcam');

      var newParent = document.getElementsByClassName('video-tag-wrapper');
	newParent[0].appendChild(document.getElementById('webcam'));

	$("#webcam").resize(function() {
	    updateVideoSize();
	});

	$(window).resize(function() {
	    updateVideoSize();
	});

	updateVideoSize();
	
	videoElem.removeClass('hide');
	videoElem.css('display', 'block');

      scope.callActive("", {useVideo: true});

      element.on('$destroy', function() {
        // Move the video back to the body.
        console.log('Moving the video back to body.');
        videoElem.addClass('hide').appendTo(jQuery('body'));
        $(window).unbind('resize');
      });
    }

    return {
      link: link
    }
  });

})();
