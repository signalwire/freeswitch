(function () {
  'use strict';

  angular
  .module('vertoDirectives').directive('userStatus',
  function() {
    var link = function(scope, element, attrs) {
      scope.$watch('condition', function(condition) {
        element.removeClass('connected');
        element.removeClass('disconnected');
        element.removeClass('connecting');

        element.addClass(condition);

      });
    }

    return {
      scope: {
        'condition': '='
      },
      link: link
    };

  });

})();