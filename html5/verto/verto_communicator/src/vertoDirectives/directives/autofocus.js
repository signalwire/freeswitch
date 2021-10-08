/*
Sometimes autofocus HTML5 directive just isn't enough with SPAs.
This directive will force autofocus to work properly under those circumstances.
*/
(function () {
  'use strict';

  angular
  .module('vertoDirectives')
  .directive('autofocus', ['$timeout',
    function ($timeout) {
      return {
        restrict: 'A',
        link: function ($scope, $element) {
          $timeout(function () {
            $element[0].focus();
          });
        }
      };
    }
  ]);
})();