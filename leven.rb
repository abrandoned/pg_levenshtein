require 'rubygems'
require 'ffi'

module Levenshtein
  extend FFI::Library
  ffi_lib "./libpg_levenshtein.so"
  attach_function :levenshtein_extern, [:pointer, :pointer, :int32], :int32
  
  def self.leven(str1, str2, max=9999)
    self.levenshtein_extern(str1, str2, max)
  end
end
