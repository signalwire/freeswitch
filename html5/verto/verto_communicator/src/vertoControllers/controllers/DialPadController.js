(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('DialPadController', ['$rootScope', '$scope',
      '$http', '$location', 'toastr', 'verto', 'storage', 'CallHistory',
      function($rootScope, $scope, $http, $location, toastr, verto, storage, CallHistory) {
        console.debug('Executing DialPadController.');
        $scope.checkBrowser();
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
        if ($location.search().autocall) {
          $rootScope.dialpadNumber = $location.search().autocall;
        }

        /**
         * used to bind click on number in call history to fill dialpad
         * 'cause inside a ng-repeat the angular isnt in ctrl scope
         */
        $scope.fillDialpadNumber = function(number) {
          $rootScope.dialpadNumber = number;
        };

        $rootScope.transfer = function() {
          if (!$rootScope.dialpadNumber) {
            return false;
          }
          verto.data.call.transfer($rootScope.dialpadNumber);
        };

        /**
         * Call to the number in the $rootScope.dialpadNumber.
         */
        $rootScope.call = function(extension) {
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

          storage.data.mutedVideo = false;
          storage.data.mutedMic = false;

          storage.data.videoCall = false;
          verto.call($rootScope.dialpadNumber);

          storage.data.called_number = $rootScope.dialpadNumber;
          CallHistory.add($rootScope.dialpadNumber, 'outbound');
          $location.path('/incall');
        }
      }
    ]);

})();
