(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('ModalSettingsController', ['$scope', '$http',
      '$location', '$modalInstance', '$rootScope', 'storage', 'verto',
      function($scope, $http, $location, $modalInstance, $rootScope, storage, verto) {
        console.debug('Executing ModalSettingsController.');

        $scope.storage = storage;
        $scope.verto = verto;
        $scope.mydata = angular.copy(storage.data);

        $scope.ok = function() {
          if ($scope.mydata.selectedSpeaker != storage.data.selectedSpeaker) {
            $rootScope.$emit('changedSpeaker', $scope.mydata.selectedSpeaker);
          }
          storage.changeData($scope.mydata);
          verto.data.instance.iceServers(storage.data.useSTUN);

          if (storage.data.autoBand) {
            $scope.testSpeed();
          }
          $modalInstance.close('Ok.');
        };

        $scope.cancel = function() {
          $modalInstance.dismiss('cancel');
        };

        $scope.refreshDeviceList = function() {
          return verto.refreshDevices();
        };

        $scope.testSpeed = function() {
          return verto.testSpeed(cb);

          function cb(data) {
            $scope.mydata.vidQual = storage.data.vidQual;
            $scope.speedMsg = 'Up: ' + data.upKPS + ' Down: ' + data.downKPS;
            $scope.$apply();
          }
        };

        $scope.resetSettings = function() {
	  if (confirm('Factory Reset Settings?')) {
            storage.factoryReset();
            $scope.logout();
            $modalInstance.close('Ok.');
	    window.location.reload();
	  };
        };

        $scope.checkAutoBand = function(option) {
          $scope.mydata.useDedenc = false;
          if (!option) {
            $scope.mydata.outgoingBandwidth = 'default';
            $scope.mydata.incomingBandwidth = 'default';
            $scope.mydata.vidQual = 'hd';
            $scope.mydata.testSpeedJoin = false;
          } else {
            $scope.mydata.testSpeedJoin = true;
          }
        };

        $scope.checkUseDedRemoteEncoder = function(option) {
          if (['0', 'default', '5120'].indexOf(option) != -1) {
            $scope.mydata.useDedenc = false;
          } else {
            $scope.mydata.useDedenc = true;
          }
        };
      }
    ]);

})();
