(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('LoginController', ['$scope', '$http', '$location',
      'verto',
      function($scope, $http, $location, verto) {
        $scope.checkBrowser();

        /**
         * using stored data (localStorage) for logon
         */
        verto.data.name = $scope.storage.data.name;
        verto.data.email = $scope.storage.data.email;
        if ($scope.storage.data.login != '' && $scope.storage.data.password != '') {
          verto.data.login = $scope.storage.data.login;
          verto.data.password = $scope.storage.data.password;
        }

        console.debug('Executing LoginController.');
      }
    ]);
    
})();