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
        };

        $scope.confChangeSpeaker = function(speakerId) {
          storage.data.selectedSpeaker = speakerId;
          $rootScope.$emit('changedSpeaker', speakerId);
        };

        $scope.screenshare = function() {
          if(verto.data.shareCall) {
            verto.screenshareHangup();
            return false;
          }
          verto.screenshare(storage.data.called_number);
        };

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
