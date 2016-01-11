(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('InCallController', ['$rootScope', '$scope',
      '$http', '$location', '$modal', '$timeout', 'toastr', 'verto', 'storage', 'prompt', 'Fullscreen',
      function($rootScope, $scope, $http, $location, $modal, $timeout, toastr,
        verto, storage, prompt, Fullscreen) {

        console.debug('Executing InCallController.');
        $scope.layout = null;
        $rootScope.dialpadNumber = '';
        $scope.callTemplate = 'partials/phone_call.html';
        $scope.dialpadTemplate = '';
        $scope.incall = true;

        if (storage.data.videoCall) {
          $scope.callTemplate = 'partials/video_call.html';
        }

        $rootScope.$on('call.conference', function(event, data) {
          $timeout(function() {
            if($scope.chatStatus) {
              $scope.openChat();
            }
            buildCanvasesData();
          });
        });

        $rootScope.$on('call.video', function(event, data) {
          $timeout(function() {
            $scope.callTemplate = 'partials/video_call.html';
          });
        });

        /**
         * toggle dialpad in incall page
         */
        $scope.toggleDialpad = function() {
          $scope.openModal('partials/dialpad_widget.html',
            'ModalDialpadController');

          /*
          if(!$scope.dialpadTemplate) {
            $scope.dialpadTemplate = 'partials/dialpad_widget.html';
          } else {
            $scope.dialpadTemplate = '';
          }
          */
        }

        /**
         * TODO: useless?
         */
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
          $scope.videoLayout = layout;
          $rootScope.$emit('changedVideoLayout', layout);
        };

        $scope.confChangeSpeaker = function(speakerId) {
          storage.data.selectedSpeaker = speakerId;
          $rootScope.$emit('changedSpeaker', speakerId);
        };

        $scope.confPopup = function(canvas_id) {
          var video = document.getElementById('webcam');
          var s = window.location.href;
          var curCall = verto.data.call.callID;
          var extension = verto.data.call.params.remote_caller_id_number;
          var width = webcam.offsetWidth;
          var height = webcam.offsetHeight + 100;
          var x = (screen.width - width)/2
          var y = (screen.height - height)/2

          s = s.replace(/\#.*/, '');
          s += "#/?sessid=random&master=" + curCall + "&watcher=true&extension=" + extension+ "&canvas_id=" + canvas_id;

          console.log("opening new window to " + s);
          var popup = window.open(s, "canvas_window_" + canvas_id, "toolbar=0,location=0,menubar=0,directories=0,width=" + width + ",height=" + height, + ',left=' + x + ',top=' + y);
          popup.moveTo(x, y);
        };

        $scope.screenshare = function() {
          if(verto.data.shareCall) {
            verto.screenshareHangup();
            return false;
          }
          verto.screenshare(storage.data.called_number);
        };

        function buildCanvasesData() {
          $scope.conf = verto.data.conf.params.laData;
          $scope.canvases = [{ id: 1, name: 'Super Canvas' }];
          for (var i = 1; i < $scope.conf.canvasCount; i++) {
            $scope.canvases.push({ id: i+1, name: 'Canvas ' + (i+1) });
          }
        }

        $scope.muteMic = verto.muteMic;
        $scope.muteVideo = verto.muteVideo;

        $rootScope.$on('ScreenShareExtensionStatus', function(event, error) {
          var pluginUrl = 'https://chrome.google.com/webstore/detail/screen-capturing/ajhifddimkapgcifgcodmmfdlknahffk';
          switch(error) {
            case 'permission-denied':
              toastr.info('Please allow the plugin in order to use Screen Share', 'Error'); break;
            case 'not-installed':
              toastr.warning('Please <a target="_blank" class="install" href="'+ pluginUrl +'">install</a> the plugin in order to use Screen Share', 'Warning', { allowHtml: true }); break;
            case 'installed-disabled':
              toastr.info('Please enable the plugin in order to use Screen Share', 'Error'); break;
            // case 'not-chrome'
            //   toastr.info('Chrome', 'Error');
          }
        });

        $timeout(function() {
          console.log('broadcast time-start incall');
          $scope.$broadcast('timer-start');
        }, 1000);

      }
    ]);
})();
