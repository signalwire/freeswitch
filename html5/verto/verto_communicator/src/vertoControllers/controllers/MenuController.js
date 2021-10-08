(function() {
  'use strict';

  angular
    .module('vertoControllers')
	.controller('MenuController', ['$scope', '$http', '$location',
	  'verto', 'storage', '$rootScope',
	  function($scope, $http, $location, verto, storage, $rootScope) {
	    console.debug('Executing MenuController.');
      $scope.storage = storage;

      $rootScope.$on('testSpeed', function(e, data) {
        var vidQual = storage.data.vidQual;
	var bwp = 4;

        $scope.bandDown = data.downKPS;
        $scope.bandUp = data.upKPS;

        if (data.downKPS < 2000) {
          bwp--;
        }

        if (data.upKPS < 2000) {
           bwp--;
        }

        $scope.iconClass = 'mdi-device-signal-wifi-4-bar green';                      

        if (bwp < 4) {
           $scope.iconClass = 'mdi-device-signal-wifi-3-bar yellow';
        } else if (bwp < 2) {
           $scope.iconClass = 'mdi-device-signal-wifi-1-bar red';
        }

        verto.videoQuality.forEach(function(vid) {
          if (vid.id == vidQual){
            $scope.vidRes = vid.label;
          }
        });

        $scope.$apply();
      });
    }
	]);

})();
