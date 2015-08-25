(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('ModalSettingsController', ['$scope', '$http',
      '$location', '$modalInstance', 'verto', 'storage',
      function($scope, $http, $location, $modalInstance, verto, storage) {
        console.debug('Executing ModalSettingsController.');

        verto.changeData(storage);
        $scope.verto = verto;
        $scope.storage = storage;

        $scope.ok = function() {
          storage.changeData(verto);
          $modalInstance.close('Ok.');
        };

        $scope.cancel = function() {
          $modalInstance.dismiss('cancel');
        };

        $scope.refreshDeviceList = function() {
          return verto.refreshDevices();
        }
      }
    ]);

})();