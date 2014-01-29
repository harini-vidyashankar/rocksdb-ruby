#include "rocksdb_rb.h"
#include "rocksdb_db_rb.h"
#include "ruby/encoding.h"
#include <iostream>

extern "C" {
#include <ruby.h>

  VALUE rocksdb_db_init(int argc, VALUE* argv, VALUE self) {
    VALUE v_db_file_name;
    VALUE v_options;
    rocksdb_pointer* db_pointer;
    rocksdb::DB* db;
    rocksdb::Options options;
    rocksdb::Status status;
    std::string db_file_name;
    bool readonly;
    
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);
    rb_scan_args(argc, argv, "11", &v_db_file_name, &v_options);

    Check_Type(v_db_file_name, T_STRING);
    db_file_name = std::string((char*)RSTRING_PTR(v_db_file_name));

    readonly = false;
    if (TYPE(v_options) == T_HASH) {
      VALUE v = rb_hash_aref(v_options, ID2SYM(rb_intern("readonly")));
      if(v == Qtrue){
	readonly = true;
      }
    }
    options.create_if_missing = true;
    if(readonly){
      status = rocksdb::DB::OpenForReadOnly(options, db_file_name, &db);
    }else{
      status = rocksdb::DB::Open(options, db_file_name, &db);
    }

    db_pointer->db = db;

    return status.ok() ? Qtrue : Qfalse;
  }

  VALUE db_alloc(VALUE klass){
    rocksdb_pointer* db_pointer = ALLOC(rocksdb_pointer);
    return Data_Wrap_Struct(klass, 0, db_free, db_pointer);
  }

  VALUE rocksdb_db_put(VALUE self, VALUE v_key, VALUE v_value) {
    Check_Type(v_key, T_STRING);
    Check_Type(v_value, T_STRING);

    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);
    
    std::string key = std::string((char*)RSTRING_PTR(v_key));
    std::string value = std::string((char*)RSTRING_PTR(v_value));

    rocksdb::Status status = db_pointer->db->Put(rocksdb::WriteOptions(), key, value);
    
    return status.ok() ? Qtrue : Qfalse;
  }

  VALUE rocksdb_db_write(VALUE self, VALUE v_write){
    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);

    rocksdb::WriteBatch *batch;
    Data_Get_Struct(v_write, rocksdb::WriteBatch, batch);

    rocksdb::Status status = db_pointer->db->Write(rocksdb::WriteOptions(), batch);
    return status.ok() ? Qtrue : Qfalse;
  }

  VALUE rocksdb_db_get(VALUE self, VALUE v_key){
    Check_Type(v_key, T_STRING);

    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);
    
    std::string key = std::string((char*)RSTRING_PTR(v_key));
    std::string value;
    rocksdb::Status status = db_pointer->db->Get(rocksdb::ReadOptions(), key, &value);    

    return (status.IsNotFound()) ? Qnil : rb_enc_str_new(value.data(), value.size(), rb_utf8_encoding());

  }


  VALUE rocksdb_db_multi_get(VALUE self, VALUE v_array){
    Check_Type(v_array, T_ARRAY);

    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);

    long i;
    VALUE *tmp = RARRAY_PTR(v_array);
    long length = RARRAY_LEN(v_array);
    std::vector<std::string> values(length);
    std::vector<rocksdb::Slice> keys(length);
    std::vector<rocksdb::Status> status;

    for(i=0; i < length; i++){
      keys[i] = rocksdb::Slice((char*)RSTRING_PTR(*tmp));
      tmp++;
    }

    status = db_pointer->db->MultiGet(rocksdb::ReadOptions(),keys,&values);
    for(i=0; i < length; i++){
      RARRAY_PTR(v_array)[i] = rb_enc_str_new(values[i].data(), values[i].size(), rb_utf8_encoding());
    }
    return v_array;
  }
  
  VALUE rocksdb_db_delete(VALUE self, VALUE v_key){
    Check_Type(v_key, T_STRING);
    
    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);

    std::string key = std::string((char*)RSTRING_PTR(v_key));
    rocksdb::Status status = db_pointer->db->Delete(rocksdb::WriteOptions(), key);
    
    return status.ok() ? Qtrue : Qfalse;
  }

  VALUE rocksdb_db_exists(VALUE self, VALUE v_key){
    Check_Type(v_key, T_STRING);

    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);

    std::string key = std::string((char*)RSTRING_PTR(v_key));
    std::string value = std::string();
    
    return db_pointer->db->KeyMayExist(rocksdb::ReadOptions(), key, &value) ? Qtrue : Qfalse;
  }  

  VALUE rocksdb_db_close(VALUE self){
    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);

    if(db_pointer->db != NULL){
      delete db_pointer->db;
      db_pointer->db = NULL;
    }
    return Qnil;
  }

  void db_free(rocksdb_pointer* db_pointer){
    if(db_pointer->db != NULL){
      delete db_pointer->db;
      db_pointer->db = NULL;
    }
    delete db_pointer;
  }

  VALUE rocksdb_db_new_iterator(VALUE self){
    rocksdb_pointer* db_pointer;
    rocksdb_iterator_pointer* rocksdb_it;

    VALUE klass;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);

    rocksdb::Iterator* it = db_pointer->db->NewIterator(rocksdb::ReadOptions());

    klass = rb_class_new_instance(0, NULL, cRocksdb_iterator);

    Data_Get_Struct(klass, rocksdb_iterator_pointer , rocksdb_it);
    rocksdb_it->it = it;
    return klass;
  }


  VALUE rocksdb_db_each(VALUE self){
    if(!rb_block_given_p()){
      return rocksdb_db_new_iterator(self);
    }
    
    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);
    rocksdb::Iterator* it = db_pointer->db->NewIterator(rocksdb::ReadOptions());

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      rb_yield(rb_enc_str_new(it->value().data(), it->value().size(), rb_utf8_encoding()));
    }
    
    delete it;
    return self;
  }

  VALUE rocksdb_db_reverse_each(VALUE self){
    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);
    rocksdb::Iterator* it = db_pointer->db->NewIterator(rocksdb::ReadOptions());

    for (it->SeekToLast(); it->Valid(); it->Prev()) {
      rb_yield(rb_enc_str_new(it->value().data(), it->value().size(), rb_utf8_encoding()));
    }
    
    delete it;
    return self;
  }

  VALUE rocksdb_db_debug(VALUE self){
    return Qnil;
  }
}
