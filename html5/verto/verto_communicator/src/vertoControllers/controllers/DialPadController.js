(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('DialPadController', ['$rootScope', '$scope',
      '$http', '$location', 'toastr', 'verto', 'storage', 'CallHistory', 'eventQueue',
      function($rootScope, $scope, $http, $location, toastr, verto, storage, CallHistory, eventQueue) {
        console.debug('Executing DialPadController.');

        eventQueue.process();

        if ($location.search().autocall) {
            $rootScope.dialpadNumber = $location.search().autocall;
            delete $location.search().autocall;
            call($rootScope.dialpadNumber);
            if($rootScope.watcher) {
              return;
            }
        }

        $scope.call_history = CallHistory.all();
        $scope.history_control = CallHistory.all_control();
        $scope.has_history = Object.keys($scope.call_history).length;
        storage.data.videoCall = false;
        storage.data.userStatus = 'connecting';
        storage.data.calling = false;

        $scope.clearCallHistory = function() {
          CallHistory.clear();
          $scope.call_history = CallHistory.all();
          $scope.history_control = CallHistory.all_control();
          $scope.has_history = Object.keys($scope.call_history).length;
          return $scope.history_control;
        };

        $scope.viewCallsList = function(calls) {
          return $scope.call_list = calls;
        };

        /**
         * fill dialpad via querystring [?autocall=\d+]
         */

	/**
	 * fill in dialpad via config.json
	 */
        if ('autocall' in verto.data) {
          $rootScope.dialpadNumber = verto.data.autocall;
	  delete verto.data.autocall;
          call($rootScope.dialpadNumber);
        }

        /**
         * used to bind click on number in call history to fill dialpad
         * 'cause inside a ng-repeat the angular isnt in ctrl scope
         */
        $scope.fillDialpadNumber = function(number) {
          $rootScope.dialpadNumber = number;
        };

        $scope.preview = function() {
          $location.path('/preview');
        };

        $rootScope.transfer = function() {
          if (!$rootScope.dialpadNumber) {
            return false;
          }
          verto.data.call.transfer($rootScope.dialpadNumber);
        };

        function call(extension) {
          storage.data.onHold = false;
          storage.data.cur_call = 0;
          $rootScope.dialpadNumber = extension;
          if (!$rootScope.dialpadNumber && storage.data.called_number) {
            $rootScope.dialpadNumber = storage.data.called_number;
            return false;
          } else if (!$rootScope.dialpadNumber && !storage.data.called_number) {
            toastr.warning('Enter an extension, please.');
            return false;
          }

          if (verto.data.call) {
            console.debug('A call is already in progress.');
            return false;
          }

          if (extension.indexOf('-canvas-') != -1) {
            $rootScope.watcher = true;
            verto.call($rootScope.dialpadNumber, null, { useCamera: false, useMic: false, caller_id_name: null, userVariables: {}, caller_id_number: null, mirrorInput: false });
            $location.path('/incall');
            return;
          }

          storage.data.mutedVideo = false;
          storage.data.mutedMic = false;

          storage.data.videoCall = false;
          verto.call($rootScope.dialpadNumber);

          storage.data.called_number = $rootScope.dialpadNumber;
          CallHistory.add($rootScope.dialpadNumber, 'outbound');
          $location.path('/incall');
        }

        /**
         * Call to the number in the $rootScope.dialpadNumber.
         */
        $scope.loading = false;
        $rootScope.call = function(extension) {
          if (!storage.data.testSpeedJoin || !$rootScope.dialpadNumber) {
            return call(extension);
          }
          $scope.loading = true;

          verto.testSpeed(function() {
            $scope.loading = false;
            call(extension);
          });
        }
      }
    ]);

})();
