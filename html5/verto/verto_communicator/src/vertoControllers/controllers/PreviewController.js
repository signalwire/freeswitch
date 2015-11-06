(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('PreviewController', ['$rootScope', '$scope',
      '$http', '$location', '$modal', '$timeout', 'toastr', 'verto', 'storage', 'prompt', 'Fullscreen',
      function($rootScope, $scope, $http, $location, $modal, $timeout, toastr,
        verto, storage, prompt, Fullscreen) {

        $scope.storage = storage;
        console.debug('Executing PreviewController.');
        var localVideo = document.getElementById('videopreview');
        var volumes = document.querySelector('#mic-meter .volumes').children;

        $scope.localVideo = function() {
          var constraints = {
            mirrored: true,
            audio: {
              optional: [{ sourceId: storage.data.selectedAudio }]
            }
          };

          if (storage.data.selectedVideo !== 'none') {
            constraints.video = {
              optional: [{ sourceId: storage.data.selectedVideo }]
            };
          }

          navigator.getUserMedia(constraints, handleMedia, function(err, data) {

          });
        };

        var audioContext = new AudioContext();
        var mediaStreamSource = null;
        var meter;
        var streamObj = {};

        function stopMedia(stream) {
          if (typeof stream == 'function') {
            stream.stop();
          } else {
            if (stream.active) {
              var tracks = stream.getTracks();
              tracks.forEach(function(track, index) {
                track.stop();
              })
            }
          }
        }
        function handleMedia(stream) {
          if (streamObj) {
            stopMedia(streamObj);
          }

          streamObj = stream;
          localVideo.src = window.URL.createObjectURL(stream);

          mediaStreamSource = audioContext.createMediaStreamSource(stream);
          meter = createAudioMeter(audioContext);
          mediaStreamSource.connect(meter);

          renderMic();
        }

        function renderMic() {
          // meter.volume;
          var n = Math.round(meter.volume * 25);
          for(var i = volumes.length -1, j = 0; i >= 0; i--, j++) {
            var el = angular.element(volumes[j]);
            if (i >= n) el.removeClass('active');
            else        el.addClass('active');
          }

          if(!verto.data.call) {
            window.requestAnimationFrame(renderMic);
          }
        }
        /**
         * TODO: useless?
         */

        $scope.refreshDeviceList = function() {
         return verto.refreshDevices();
        };

        $scope.videoCall = function() {
          prompt({
            title: 'Would you like to activate video for this call?',
            message: 'Video will be active during the next calls.'
          }).then(function() {
            storage.data.videoCall = true;
            $scope.callTemplate = 'partials/video_call.html';
          });
        };

        $scope.cbMuteVideo = function(event, data) {
          storage.data.mutedVideo = !storage.data.mutedVideo;
        }

        $scope.cbMuteMic = function(event, data) {
          storage.data.mutedMic = !storage.data.mutedMic;
        }

        $scope.confChangeVideoLayout = function(layout) {
          verto.data.conf.setVideoLayout(layout);
        };

        $scope.endPreview = function() {
          localVideo.src = null;
          meter.shutdown();
          meter.onaudioprocess = null;
          stopMedia(streamObj);
          $location.path('/dialpad');
          storage.data.preview = false;
        };

        $scope.localVideo();
      }
    ]);
})();
