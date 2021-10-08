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

        var name = $location.search().name;
        var email = $location.search().email;
        var skipPreview = $location.search().skipPreview;

        if (name && email) {
          verto.data.name = name;
          verto.data.email = email;
          $scope.login(true, skipPreview);
          return;
        }

        verto.data.name = $scope.storage.data.name;
        verto.data.email = $scope.storage.data.email;

        console.debug('Executing LoginController.');
      }
    ]);

})();

