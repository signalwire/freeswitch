'use strict';

/* Filters */

var vertoFilters = angular.module('vertoFilters', []);

vertoFilters.filter('gravatar',
  function() {
    return function (email, size) {
      if (angular.isUndefined(size)) {
        size = 40;
      }
      var hash = md5(email);
      return 'https://secure.gravatar.com/avatar/' + hash + '?s=' + size + '&d=mm';
    }
  });
