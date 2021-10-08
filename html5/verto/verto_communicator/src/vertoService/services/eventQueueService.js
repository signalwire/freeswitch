'use strict';

  angular
    .module('vertoService')
    .service('eventQueue', ['$rootScope', '$q', 'storage', 'verto',
      function($rootScope, $q, storage, verto) {
        
        var events = [];
        
        var next = function() {
          var fn, fn_return;
          
          fn = events.shift();
          
          if (fn == undefined) {
            $rootScope.$emit('eventqueue.complete');
            return;
          }
          fn_return = fn();

          var emitNextProgress = function() {
            $rootScope.$emit('eventqueue.next');
          };

          fn_return.then(
            function() {
              emitNextProgress();
            }, 
            function() {
              emitNextProgress();
            }
          );
        };

        var process = function() {
          $rootScope.$on('eventqueue.next', function (ev){
            next();
          });
          
          next(); 
        };

        return {
          'next': next,
          'process': process,
          'events': events
        };

      }]);
 
