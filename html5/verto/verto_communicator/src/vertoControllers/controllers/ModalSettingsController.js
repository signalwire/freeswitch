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
        }

        $scope.resetSettings = function() {
          storage.factoryReset();
        }
      }
    ]);

})();
