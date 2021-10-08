(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('ModalDialpadController', ['$scope',
      '$modalInstance',
      function($scope, $modalInstance) {

        $scope.ok = function() {
          $modalInstance.close('Ok.');
        };

        $scope.cancel = function() {
          $modalInstance.dismiss('cancel');
        };

      }
    ]);
})();