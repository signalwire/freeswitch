<?php
##
##  OSSP uuid - Universally Unique Identifier
##  Copyright (c) 2004-2007 Ralf S. Engelschall <rse@engelschall.com>
##  Copyright (c) 2004-2007 The OSSP Project <http://www.ossp.org/>
##
##  This file is part of OSSP uuid, a library for the generation
##  of UUIDs which can found at http://www.ossp.org/pkg/lib/uuid/
##
##  Permission to use, copy, modify, and distribute this software for
##  any purpose with or without fee is hereby granted, provided that
##  the above copyright notice and this permission notice appear in all
##  copies.
##
##  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
##  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
##  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
##  IN NO EVENT SHALL THE AUTHORS AND COPYRIGHT HOLDERS AND THEIR
##  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
##  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
##  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
##  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
##  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
##  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
##  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
##  SUCH DAMAGE.
##
##  uuid.php: PHP/Zend API (language: php 5.x)
##

class UUID {
    private $uuid = null;
    public function __construct() {
        uuid_create(&$this->uuid);
    }
    public function __destruct() {
        uuid_destroy($this->uuid);
    }
    public function __clone() {
        $uuid = null;
        uuid_clone($this->uuid, &$uuid);
        $this->uuid = $uuid;
    }
    public function load($name) {
        uuid_load($this->uuid, $name);
    }
    public function make($fmt, $ns = null, $url = null) {
        if (func_num_args() == 3) {
            uuid_make($this->uuid, $fmt, $ns->uuid, $url);
        }
        else {
            uuid_make($this->uuid, $fmt);
        }
    }
    public function isnil() {
        $result = 0;
        uuid_isnil($this->uuid, &$result);
        return $result;
    }
    public function compare($other) {
        $result = 0;
        uuid_compare($this->uuid, $other->uuid, &$result);
        return $result;
    }
    public function import($fmt, $data) {
        uuid_import($this->uuid, $fmt, $data);
    }
    public function export($fmt) {
        $data = "";
        uuid_export($this->uuid, $fmt, &$data);
        return $data;
    }
    public function error($rc) {
        return uuid_error($this->uuid, $rc);
    }
    public function version() {
        return uuid_version();
    }
}

?>
