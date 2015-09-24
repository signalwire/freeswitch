(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('LoginController', ['$scope', '$http', '$location', 'verto', 
      function($scope, $http, $location, verto) {
        var preRoute = function() {
          if(verto.data.connected) {
            $location.path('/dialpad');
          }
        }
        preRoute();
        
        verto.data.name = $scope.storage.data.name;
        verto.data.email = $scope.storage.data.email;

        console.debug('Executing LoginController.');
      }
    ]);

})();

