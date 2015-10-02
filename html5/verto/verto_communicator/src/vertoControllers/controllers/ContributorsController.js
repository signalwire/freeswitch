(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('ContributorsController', ['$scope', '$http',
      'toastr',
      function($scope, $http, toastr) {
        var url = window.location.origin + window.location.pathname;
        $http.get(url + 'contributors.txt')
          .success(function(data) {

            var contributors = [];

            angular.forEach(data, function(value, key) {
              var re = /(.*) <(.*)>/;
              var name = value.replace(re, "$1");
              var email = value.replace(re, "$2");

              this.push({
                'name': name,
                'email': email
              });
            }, contributors);

            $scope.contributors = contributors;
          })
          .error(function() {
            toastr.error('contributors not found.');
          });
      }
    ]);
})();
