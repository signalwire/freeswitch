(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('AboutController', ['$scope', '$http',
      'toastr',
      function($scope, $http, toastr) {
	    var githash = '/* @echo revision */' || 'something is not right';
            $scope.githash = githash;

	/* leave this here for later, but its not needed right now
        $http.get(window.location.pathname + '/contributors.txt')
          .success(function(data) {

          })
          .error(function() {
            toastr.error('contributors not found.');
          });
	 */
      }
    ]);
})();
