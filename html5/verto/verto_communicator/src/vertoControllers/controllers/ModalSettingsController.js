(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('ModalSettingsController', ['$scope', '$http',
      '$location', '$modalInstance', 'storage', 'verto',
      function($scope, $http, $location, $modalInstance, storage, verto) {
        console.debug('Executing ModalSettingsController.');

        $scope.storage = storage;
        $scope.verto = verto;
        $scope.mydata = angular.copy(storage.data);

        $scope.ok = function() {
          storage.changeData($scope.mydata);
          verto.data.instance.iceServers(storage.data.useSTUN);
          $modalInstance.close('Ok.');
        };

        $scope.cancel = function() {
          $modalInstance.dismiss('cancel');
        };

        $scope.refreshDeviceList = function() {
          return verto.refreshDevices();
        };

        $scope.rangeBandwidth = function(bandwidth) {
          for(var i = 0; i < verto.videoQuality.length; i++) {

          }
        };

        $scope.testSpeed = function() {
          return verto.testSpeed(cb);

          function cb(data) {

            $scope.$apply();
          }
        };

        $scope.resetSettings = function() {
	  if (confirm('Factory Reset Settings?')) {
            storage.factoryReset();
            $scope.logout();
            $scope.ok();
	    window.location.reload();
	  };
        };

        $scope.checkUseDedRemoteEncoder = function(option) {
          if ($scope.mydata.incomingBandwidth != 'default' || $scope.mydata.outgoingBandwidth != 'default') {
            $scope.mydata.useDedenc = true;
          } else {
            $scope.mydata.useDedenc = false;
          }
        };
      }
    ]);

})();
