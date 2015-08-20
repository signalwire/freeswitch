/*jslint node: true */
'use strict';

var pkg = require('./package.json');

module.exports = function (grunt) {

  // load all grunt tasks
  require('load-grunt-tasks')(grunt);

  // Project configuration.
  grunt.initConfig({
    watch: {
      js: {
        files: ['js/{,*/}*.js'],
        tasks: ['newer:jshint:all'],
        options: {
          livereload: true
        }
      },
      gruntfile: {
        files: ['Gruntfile.js']
      },
      livereload: {
        options: {
          livereload: true
        },
        files: [
          'index.html',
          'partials/{,*/}*.html',
          'js/{,*/}*.js',
          'images/{,*/}*.{png,jpg,jpeg,gif,webp,svg}'
        ]
      }
    },
    wiredep: {
      app: {
        src: ['index.html'],
        ignorePath:  /\.\.\//
      }
    },
    connect: {
      options: {
        port: 9001,
        // Change this to '0.0.0.0' to access the server from outside.
        hostname: 'localhost',
        livereload: 35729
      },
      livereload: {
        options: {
          open: false,
          middleware: function (connect) {
            return [
              connect().use(
                '/js/src',
                connect.static('../js/src')
              ),
              connect.static('.')
            ];
          }
        }
      },
    },
  });

  grunt.registerTask('serve', ['wiredep', 'connect:livereload', 'watch']);
};
