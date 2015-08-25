(function() {
  'use strict';

  angular
    .module('vertoControllers')
	.controller('MenuController', ['$scope', '$http', '$location',
	  'verto', 'storage',
	  function($scope, $http, $location, verto, storage) {
	    console.debug('Executing MenuController.');
	  }
	]);

})();