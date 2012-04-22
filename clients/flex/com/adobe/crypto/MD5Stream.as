/*
  Copyright (c) 2008, Adobe Systems Incorporated
  All rights reserved.

  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright notice, 
    this list of conditions and the following disclaimer.
  
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the 
    documentation and/or other materials provided with the distribution.
  
  * Neither the name of Adobe Systems Incorporated nor the names of its 
    contributors may be used to endorse or promote products derived from 
    this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

package com.adobe.crypto
{
    import com.adobe.utils.IntUtil;   
    import flash.utils.ByteArray;

    /**
     * Perform MD5 hash of an input stream in chunks. This class is
     * based on com.adobe.crypto.MD5 and can process data in
     * chunks. Both block creation and hash computation are done
     * together for whatever input is available so that the memory
     * overhead at a time is always fixed. Memory usage is governed by
     * two parameters: one is the amount of data passed in to update()
     * and the other is memoryBlockSize. The latter comes into play
     * only when the memory window exceeds the pre allocated memory
     * window of flash player. Usage: create an instance, call
     * update(data) repeatedly for all chunks and finally complete()
     * which will return the md5 hash.
     */      
    public class MD5Stream
    {
        private static var mask:int = 0xFF;

        private var arr:Array = [];

        /* running count of length */
        private var arrLen:int;
        
        // initialize the md buffers
        private var a:int = 1732584193;
        private var b:int = -271733879;
        private var c:int = -1732584194;
        private var d:int = 271733878;
        
        // variables to store previous values
        private var aa:int;
        private var bb:int;
        private var cc:int;
        private var dd:int;

        /* index for data read */
        private var arrIndexLen:int = 0;
        /* index for hash computation */
        private var arrProcessIndex:int = 0;
        /* index for removing stale arr values */
        private var cleanIndex:int = 0;
        
        /** 
         * Change this value from the default (16384) in the range of
         * MBs to actually affect GC as GC allocates in pools of
         * memory */
        public var memoryBlockSize:int = 16384;
        
        
        public function MD5Stream()
        {
            
        }
               
        
        /**
         * Pass in chunks of the input data with update(), call
         * complete() with an optional chunk which will return the
         * final hash. Equivalent to the way
         * java.security.MessageDigest works.
         *
         * @param input The optional bytearray chunk which is the final part of the input
         * @return A string containing the hash value
         * @langversion ActionScript 3.0
         * @playerversion Flash 8.5
         * @tiptext
         */
        public function complete(input:ByteArray=null):String
        {
            if ( arr.length == 0 )
            {
                if ( input == null )
                {
                    throw new Error("null input to complete without prior call to update. At least an empty bytearray must be passed.");
                }		 		
            }
            
            if ( input != null )
            {
                readIntoArray(input);
            }

            //pad, append length
            padArray(arrLen);

            hashRemainingChunks(false);
            
            var res:String = IntUtil.toHex( a ) + IntUtil.toHex( b ) + 
            				 IntUtil.toHex( c ) + IntUtil.toHex( d );
            resetFields();
            
            return res;
        }

        /**
         * Pass in chunks of the input data with update(), call
         * complete() with an optional chunk which will return the
         * final hash. Equivalent to the way
         * java.security.MessageDigest works.
         *
         * @param input The bytearray chunk to perform the hash on
         * @langversion ActionScript 3.0
         * @playerversion Flash 8.5
         * @tiptext
         */        
        public function update(input:ByteArray):void
        {
            readIntoArray(input);
            hashRemainingChunks();
        }

        /**
         * Re-initialize this instance for use to perform hashing on
         * another input stream. This is called automatically by
         * complete().
         *
         * @langversion ActionScript 3.0
         * @playerversion Flash 8.5
         * @tiptext
         */               
        public function resetFields():void
        {
            //truncate array
            arr.length = 0;
            arrLen = 0;
            
            // initialize the md buffers
            a = 1732584193;
            b = -271733879;
            c = -1732584194;
            d = 271733878;
            
            // variables to store previous values
            aa = 0;
            bb = 0;
            cc = 0;
            dd = 0;
            
            arrIndexLen = 0;            
            arrProcessIndex = 0;
            cleanIndex = 0;
        }
        
        /** read into arr and free up used blocks of arr */
        private function readIntoArray(input:ByteArray):void
        {
            var closestChunkLen:int = input.length * 8;
            arrLen += closestChunkLen;
            
            /* clean up memory. if there are entries in the array that
             * are already processed and the amount is greater than
             * memoryBlockSize, create a new array, copy the last
             * block into it and let the old one get picked up by
             * GC. */
            if ( arrProcessIndex - cleanIndex > memoryBlockSize )
            {
                var newarr:Array= new Array();
                
                /* AS Arrays in sparse arrays. arr[2002] can exist 
                 * without values for arr[0] - arr[2001] */
                for ( var j:int = arrProcessIndex; j < arr.length; j++ )
                {						
                    newarr[j] = arr[j];
                }
                
                cleanIndex = arrProcessIndex;
                arr = null;
                arr = newarr;
            }
            
            for ( var k:int = 0; k < closestChunkLen; k+=8 )
            {
                //discard high bytes (convert to uint)
                arr[ int(arrIndexLen >> 5) ] |= ( input[ k / 8 ] & mask ) << ( arrIndexLen % 32 );
                arrIndexLen += 8;
            }
            
            
        }
        
        private function hashRemainingChunks(bUpdate:Boolean=true):void
        {
            var len:int = arr.length;

            /* leave a 16 word block untouched if we are called from
             * update. This is because, padArray() can modify the last
             * block and this modification has to happen before we
             * compute the hash.  */
            if ( bUpdate )
            {
                len -= 16;
            }

            /* don't do anything if don't have a 16 word block. */
            if ( arrProcessIndex >= len || len - arrProcessIndex < 15 )
            {
                return;
            }

            
            for ( var i:int = arrProcessIndex; i < len ; i += 16, arrProcessIndex += 16) 
            {            	
                // save previous values
                aa = a;
                bb = b;
                cc = c;
                dd = d;                         
                
                // Round 1
                a = ff( a, b, c, d, arr[int(i+ 0)],  7, -680876936 );     // 1
                d = ff( d, a, b, c, arr[int(i+ 1)], 12, -389564586 );     // 2
                c = ff( c, d, a, b, arr[int(i+ 2)], 17, 606105819 );      // 3
                b = ff( b, c, d, a, arr[int(i+ 3)], 22, -1044525330 );    // 4
                a = ff( a, b, c, d, arr[int(i+ 4)],  7, -176418897 );     // 5
                d = ff( d, a, b, c, arr[int(i+ 5)], 12, 1200080426 );     // 6
                c = ff( c, d, a, b, arr[int(i+ 6)], 17, -1473231341 );    // 7
                b = ff( b, c, d, a, arr[int(i+ 7)], 22, -45705983 );      // 8
                a = ff( a, b, c, d, arr[int(i+ 8)],  7, 1770035416 );     // 9
                d = ff( d, a, b, c, arr[int(i+ 9)], 12, -1958414417 );    // 10
                c = ff( c, d, a, b, arr[int(i+10)], 17, -42063 );                 // 11
                b = ff( b, c, d, a, arr[int(i+11)], 22, -1990404162 );    // 12
                a = ff( a, b, c, d, arr[int(i+12)],  7, 1804603682 );     // 13
                d = ff( d, a, b, c, arr[int(i+13)], 12, -40341101 );      // 14
                c = ff( c, d, a, b, arr[int(i+14)], 17, -1502002290 );    // 15
                b = ff( b, c, d, a, arr[int(i+15)], 22, 1236535329 );     // 16
                
                // Round 2
                a = gg( a, b, c, d, arr[int(i+ 1)],  5, -165796510 );     // 17
                d = gg( d, a, b, c, arr[int(i+ 6)],  9, -1069501632 );    // 18
                c = gg( c, d, a, b, arr[int(i+11)], 14, 643717713 );      // 19
                b = gg( b, c, d, a, arr[int(i+ 0)], 20, -373897302 );     // 20
                a = gg( a, b, c, d, arr[int(i+ 5)],  5, -701558691 );     // 21
                d = gg( d, a, b, c, arr[int(i+10)],  9, 38016083 );       // 22
                c = gg( c, d, a, b, arr[int(i+15)], 14, -660478335 );     // 23
                b = gg( b, c, d, a, arr[int(i+ 4)], 20, -405537848 );     // 24
                a = gg( a, b, c, d, arr[int(i+ 9)],  5, 568446438 );      // 25
                d = gg( d, a, b, c, arr[int(i+14)],  9, -1019803690 );    // 26
                c = gg( c, d, a, b, arr[int(i+ 3)], 14, -187363961 );     // 27
                b = gg( b, c, d, a, arr[int(i+ 8)], 20, 1163531501 );     // 28
                a = gg( a, b, c, d, arr[int(i+13)],  5, -1444681467 );    // 29
                d = gg( d, a, b, c, arr[int(i+ 2)],  9, -51403784 );      // 30
                c = gg( c, d, a, b, arr[int(i+ 7)], 14, 1735328473 );     // 31
                b = gg( b, c, d, a, arr[int(i+12)], 20, -1926607734 );    // 32
                
                // Round 3
                a = hh( a, b, c, d, arr[int(i+ 5)],  4, -378558 );        // 33
                d = hh( d, a, b, c, arr[int(i+ 8)], 11, -2022574463 );    // 34
                c = hh( c, d, a, b, arr[int(i+11)], 16, 1839030562 );     // 35
                b = hh( b, c, d, a, arr[int(i+14)], 23, -35309556 );      // 36
                a = hh( a, b, c, d, arr[int(i+ 1)],  4, -1530992060 );    // 37
                d = hh( d, a, b, c, arr[int(i+ 4)], 11, 1272893353 );     // 38
                c = hh( c, d, a, b, arr[int(i+ 7)], 16, -155497632 );     // 39
                b = hh( b, c, d, a, arr[int(i+10)], 23, -1094730640 );    // 40
                a = hh( a, b, c, d, arr[int(i+13)],  4, 681279174 );      // 41
                d = hh( d, a, b, c, arr[int(i+ 0)], 11, -358537222 );     // 42
                c = hh( c, d, a, b, arr[int(i+ 3)], 16, -722521979 );     // 43
                b = hh( b, c, d, a, arr[int(i+ 6)], 23, 76029189 );       // 44
                a = hh( a, b, c, d, arr[int(i+ 9)],  4, -640364487 );     // 45
                d = hh( d, a, b, c, arr[int(i+12)], 11, -421815835 );     // 46
                c = hh( c, d, a, b, arr[int(i+15)], 16, 530742520 );      // 47
                b = hh( b, c, d, a, arr[int(i+ 2)], 23, -995338651 );     // 48
                
                // Round 4
                a = ii( a, b, c, d, arr[int(i+ 0)],  6, -198630844 );     // 49
                d = ii( d, a, b, c, arr[int(i+ 7)], 10, 1126891415 );     // 50
                c = ii( c, d, a, b, arr[int(i+14)], 15, -1416354905 );    // 51
                b = ii( b, c, d, a, arr[int(i+ 5)], 21, -57434055 );      // 52
                a = ii( a, b, c, d, arr[int(i+12)],  6, 1700485571 );     // 53
                d = ii( d, a, b, c, arr[int(i+ 3)], 10, -1894986606 );    // 54
                c = ii( c, d, a, b, arr[int(i+10)], 15, -1051523 );       // 55
                b = ii( b, c, d, a, arr[int(i+ 1)], 21, -2054922799 );    // 56
                a = ii( a, b, c, d, arr[int(i+ 8)],  6, 1873313359 );     // 57
                d = ii( d, a, b, c, arr[int(i+15)], 10, -30611744 );      // 58
                c = ii( c, d, a, b, arr[int(i+ 6)], 15, -1560198380 );    // 59
                b = ii( b, c, d, a, arr[int(i+13)], 21, 1309151649 );     // 60
                a = ii( a, b, c, d, arr[int(i+ 4)],  6, -145523070 );     // 61
                d = ii( d, a, b, c, arr[int(i+11)], 10, -1120210379 );    // 62
                c = ii( c, d, a, b, arr[int(i+ 2)], 15, 718787259 );      // 63
                b = ii( b, c, d, a, arr[int(i+ 9)], 21, -343485551 );     // 64
                
                a += aa;
                b += bb;
                c += cc;
                d += dd;
                
            }
            
        }
        
        private function padArray(len:int):void
        {	 		
            arr[ int(len >> 5) ] |= 0x80 << ( len % 32 );
            arr[ int(( ( ( len + 64 ) >>> 9 ) << 4 ) + 14) ] = len;
            arrLen = arr.length;
        }  
        
        /* Code below same as com.adobe.crypto.MD5 */ 
        
        /**
         * Auxiliary function f as defined in RFC
         */
        private static function f( x:int, y:int, z:int ):int {
            return ( x & y ) | ( (~x) & z );
        }
        
        /**
         * Auxiliary function g as defined in RFC
         */
        private static function g( x:int, y:int, z:int ):int {
            return ( x & z ) | ( y & (~z) );
        }
        
        /**
         * Auxiliary function h as defined in RFC
         */
        private static function h( x:int, y:int, z:int ):int {
            return x ^ y ^ z;
        }
        
        /**
         * Auxiliary function i as defined in RFC
         */
        private static function i( x:int, y:int, z:int ):int {
            return y ^ ( x | (~z) );
        }
        
        /**
         * A generic transformation function.  The logic of ff, gg, hh, and
         * ii are all the same, minus the function used, so pull that logic
         * out and simplify the method bodies for the transoformation functions.
         */
        private static function transform( func:Function, a:int, b:int, c:int, d:int, x:int, s:int, t:int):int {
            var tmp:int = a + int( func( b, c, d ) ) + x + t;
            return IntUtil.rol( tmp, s ) +  b;
        }
        
        /**
         * ff transformation function
         */
        private static function ff ( a:int, b:int, c:int, d:int, x:int, s:int, t:int ):int {
            return transform( f, a, b, c, d, x, s, t );
        }
        
        /**
         * gg transformation function
         */
        private static function gg ( a:int, b:int, c:int, d:int, x:int, s:int, t:int ):int {
            return transform( g, a, b, c, d, x, s, t );
        }
        
        /**
         * hh transformation function
         */
        private static function hh ( a:int, b:int, c:int, d:int, x:int, s:int, t:int ):int {
            return transform( h, a, b, c, d, x, s, t );
        }
        
        /**
         * ii transformation function
         */
        private static function ii ( a:int, b:int, c:int, d:int, x:int, s:int, t:int ):int {
            return transform( i, a, b, c, d, x, s, t );
        }
        
    }
}