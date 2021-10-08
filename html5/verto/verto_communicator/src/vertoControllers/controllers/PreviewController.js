(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('PreviewController', ['$rootScope', '$scope',
      '$http', '$location', '$modal', '$timeout', 'toastr', 'verto', 'storage', 'prompt', 'Fullscreen', '$translate',
      function($rootScope, $scope, $http, $location, $modal, $timeout, toastr,
        verto, storage, prompt, Fullscreen, $translate) {

        $scope.storage = storage;
        console.debug('Executing PreviewController.');
        var localVideo = document.getElementById('videopreview');
        var volumes = document.querySelector('#mic-meter .volumes');
        if (volumes) {
          volumes = volumes.children;
        }

        $scope.localVideo = function() {
          var constraints = {
            mirrored: true,
            audio: {
              optional: [{ sourceId: storage.data.selectedAudio }]
            }
          };

          var newDevice = verto.data.videoDevices.find(function(device) {
            return device.id == storage.data.selectedVideo;
          });

          storage.data.selectedVideo = newDevice.id;
          storage.data.selectedVideoName = newDevice.label;

          if (newDevice.id !== 'none') {
            constraints.video = {
              optional: [{ sourceId: newDevice.id }]
            };
          }

          navigator.getUserMedia(constraints, handleMedia, function(err, data) {

          });
        };
        var audioContext = null;
        if (typeof AudioContext !== "undefined") {
          audioContext = new AudioContext();
        }

        var mediaStreamSource = null;
        var meter = null;
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
          FSRTCattachMediaStream(localVideo, stream);
          if (audioContext) {
            mediaStreamSource = audioContext.createMediaStreamSource(stream);
            meter = createAudioMeter(audioContext);
            mediaStreamSource.connect(meter);
          };
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
            title: $translate.instant('TITLE_ENABLE_VIDEO'),
            message: $translate.instant('MESSAGE_ENABLE_VIDEO')
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
          if (audioContext) {
            meter.shutdown();
            meter.onaudioprocess = null;
          };
          stopMedia(streamObj);
          $location.path('/dialpad');
          storage.data.preview = false;
        };

        $scope.localVideo();
      }
    ]);
})();
