(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('ModalLoginInformationController', ['$scope',
      '$http', '$location', '$modalInstance', 'verto', 'storage',
      function($scope, $http, $location, $modalInstance, verto, storage) {
        console.debug('Executing ModalLoginInformationController.');

        $scope.verto = verto;
        $scope.storage = storage;

        $scope.ok = function() {
          $modalInstance.close('Ok.');
        };

        $scope.cancel = function() {
          $modalInstance.dismiss('cancel');
        };

      }
    ]);
    
})();