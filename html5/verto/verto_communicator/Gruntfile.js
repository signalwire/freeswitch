/*jslint node: true */
'use strict';

// var pkg = require('./package.json');

module.exports = function (grunt) {

  // Time how long tasks take. Can help when optimizing build times
  require('time-grunt')(grunt);
  
  // load all grunt tasks
  require('jit-grunt')(grunt, {
    includereplace: 'grunt-include-replace',
    useminPrepare: 'grunt-usemin'
  });

  // Configurable paths
  var config = {
    app: './src',
    dist: 'dist'
  };

  var ip = grunt.option('ip');
  var debug = grunt.option('debug');

  var uglify_config = {
    options: {
      sourceMap: true,
      sourceMapIncludeSources:true
    }
  };
  
  if (debug) {
    uglify_config['options']['mangle'] = debug ? false : true;
    uglify_config['options']['beautify'] = debug ? false : true;
    uglify_config['options']['compress'] = debug ? false : true;
  }
  // Project configuration.
  grunt.initConfig({
    uglify: uglify_config,
    // Project settings
    config: config,

    // Watch things 
    watch: {
      bower: {
        files: ['bower.json'],
        tasks: ['wiredep']
      },
      styles: {
        files: ['<%= config.app %>/css/{,*/}*.css'],
        tasks: ['newer:copy:styles', 'postcss']
      },
      gruntfile: {
        files: ['Gruntfile.js']
      }
    },

    wiredep: {
      app: {
        src: ['src/index.html'],
        ignorePath:  /\.\.\//
      }
    },

    revision: {
      options: {
        property: 'meta.revision',
        ref: 'HEAD',
        short: true
      }
    },
    
    preprocess: {
      options: {
	context: {
          revision: '<%= meta.revision %>' 
	}
      },
      js: {
	src: 'src/vertoControllers/controllers/AboutController.source.js',
	dest: 'src/vertoControllers/controllers/AboutController.js'
      },
    },

    postcss: {
      options: {
        map: true,
        processors: [
          // Add vendor prefixed styles
          require('autoprefixer-core')({
            browsers: ['> 1%', 'last 2 versions', 'Firefox ESR', 'Opera 12.1']
          })
        ]
      },
      dist: {
        files: [{
          expand: true,
          cwd: '.tmp/css/',
          src: '{,*/}*.css',
          dest: '.tmp/css/'
        }]
      }
    },

    browserSync: {
      options: {
        notify: false,
        background: true,
        https: true,
        open: false,
        // logLevel: "debug",
        ghostMode: false,
        logConnections: true,
        ui: false
      },
      livereload: {
        options: {
          files: [
            '<%= config.app %>/**/*.html',
            '.tmp/styles/{,*/}*.css',
            '<%= config.app %>/images/{,*/}*',
            '<%= config.app %>/locales/{,*/}*',
            '.tmp/**/*.js',
            '<%= config.app %>/**/*.js'
          ],
          port: 9001,
          server: {
            baseDir: ['../js/src/', './js', '.'],
            index: 'src/index.html',
            middleware: [
              require("connect-logger")(),
              function(req, res, next) {
                if (ip) {
                  var parsed = require("url").parse(req.url);
                  if (!parsed.pathname.match(/vertoService\.js$/)) {
                    next();
                    return;
                  } else {
                    grunt.log.writeln('Providing replaced data.');
                    var path = require('path');
                    var theFilePath = path.resolve('./src', 'vertoService', 'services', 'vertoService.js');
                    var f = require('fs').readFileSync(theFilePath).toString();
                    res.setHeader('Content-Type', 'text/javascript');
                    res.end(f.replace(/window\.location\.hostname/gi, ip));
                    return;
                  }
                }
                next();
              }
            ],
            routes: {
              '/partials': 'src/partials',
              '/locales': 'src/locales',
              '/config.json': 'src/config.json',
              '/contributors.txt': 'src/contributors.txt',
              '/bower_components': './bower_components',
              '/js/src': '../js/src',
              '/js': './js'
            }
          }
        }
      },
      dist: {
        options: {
          port: 9001,
          background: false,
          server: {
            baseDir: ['dist']
          }
        }
      }
    },

    jshint: {
      options: {
        jshintrc: '.jshintrc',
        reporter: require('jshint-stylish'),
        ignores: ['js/3rd-party/**/*.js'],
        force: true // TODO: Remove this once we get files linted correctly!!!
      },
      all: {
        src: [
          'Gruntfile.js',
          'src/**/*.js'
        ]
      }
    },
    clean: {
      dist: {
        files: [{
          dot: true,
          src: [
            '.tmp',
            'dist/{,*/}*',
            '!dist/.git{,*/}*'
            ]
          }]
        }
    },
    // Renames files for browser caching purposes
    filerev: {
      dist: {
        src: [
          'dist/scripts/{,*/}*.js',
          'dist/css/{,*/}*.css',
          'dist/images/{,*/}*.{png,jpg,jpeg,gif,webp,svg}',
          'dist/css/fonts/*'
        ]
      }
    },
    // Reads HTML for usemin blocks to enable smart builds that automatically
    // concat, minify and revision files. Creates configurations in memory so
    // additional tasks can operate on them
    useminPrepare: {
      options: {
        dest: '<%= config.dist %>'
      },
      html: '<%= config.app %>/index.html'
    },

    // Performs rewrites based on rev and the useminPrepare configuration
    usemin: {
      options: {
        assetsDirs: [
          '<%= config.dist %>',
          '<%= config.dist %>/images',
          '<%= config.dist %>/styles'
        ]
      },
      html: ['<%= config.dist %>/**/*.html'],
      css: ['<%= config.dist %>/styles/{,*/}*.css']
    },

    // The following *-min tasks produce minified files in the dist folder
    imagemin: {
      dist: {
        files: [{
          expand: true,
          cwd: '<%= config.app %>/images',
          src: '{,*/}*.{gif,jpeg,jpg,png}',
          dest: '<%= config.dist %>/images'
        }]
      }
    },

    svgmin: {
      dist: {
        files: [{
          expand: true,
          cwd: '<%= config.app %>/images',
          src: '{,*/}*.svg',
          dest: '<%= config.dist %>/images'
        }]
      }
    },

    // htmlmin: {
    //   dist: {
    //     options: {
    //       collapseBooleanAttributes: true,
    //       collapseWhitespace: true,
    //       conservativeCollapse: true,
    //       removeAttributeQuotes: true,
    //       removeCommentsFromCDATA: true,
    //       removeEmptyAttributes: true,
    //       removeOptionalTags: true,
    //       // true would impact styles with attribute selectors
    //       removeRedundantAttributes: false,
    //       useShortDoctype: true
    //     },
    //     files: [{
    //       expand: true,
    //       cwd: '<%= config.dist %>',
    //       src: '{,*/}*.html',
    //       dest: '<%= config.dist %>'
    //     },
    //     {
    //       expand: true,
    //       cwd: '<%= config.dist %>/partials',
    //       src: '{,*/}*.html',
    //       dest: '<%= config.dist %>/partials'
    //     }]
    //   }
    // },
     // ng-annotate tries to make the code safe for minification automatically
     // by using the Angular long form for dependency injection.
     ngAnnotate: {
       dist: {
         files: [{
           expand: true,
           cwd: '.tmp/concat/scripts',
           src: '*.js',
           dest: '.tmp/concat/scripts'
         }]
       }
     },
     // Copies remaining files to places other tasks can use
     copy: {
       dist: {
         files: [{
           expand: true,
           dot: true,
           cwd: '<%= config.app %>',
           dest: 'dist',
           src: [
             '*.{ico,png,txt}',
             '*.html',
             '*.json',
             'partials/**/*.html',
             'img/*.png',
             'images/{,*/}*.{webp}',
             'css/fonts/{,*/}*.*',
             'sounds/*.*',
             'locales/*.*'
           ]
         }, {
           expand: true,
           cwd: '.tmp/images',
           dest: 'dist/images',
           src: ['generated/*']
         }, {
           expand: true,
           cwd: 'bower_components/bootstrap/dist',
           src: 'fonts/*',
           dest: 'dist'
         }, {
           expand: true,
           cwd: 'bower_components/bootstrap-material-design/dist',
           src: 'fonts/*',
           dest: 'dist'
         }]
       },
       styles: {
         expand: true,
         cwd: '<%= config.app %>/css',
         dest: '.tmp/css/',
         src: '{,*/}*.css'
       }
     },
     // Run some tasks in parallel to speed up the build process
     concurrent: {
       server: [
         'copy:styles'
       ],
       dist: [
         'copy:styles',
         'imagemin',
         'svgmin'
       ]
     },
  });

  grunt.loadNpmTasks('grunt-git-revision');
  grunt.loadNpmTasks('grunt-preprocess');

  grunt.registerTask('serve', function (target) {
    var tasks = [
      'wiredep',
      'concurrent:server',
      'postcss'];

    
    tasks = tasks.concat(['browserSync:livereload',
    'watch']);
    
    grunt.task.run(tasks);
  });

  grunt.registerTask('default', ['build']);
  
  grunt.registerTask('build', [
    'clean:dist',
    'revision',
    'preprocess',
    'wiredep',
    'useminPrepare',
    'concurrent:dist',
    'postcss',
    'concat',
    'cssmin',
    'ngAnnotate',
    'uglify',
    'copy:dist',
    'filerev',
    'usemin',
    // 'htmlmin'
  ]);

};
