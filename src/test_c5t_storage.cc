#include <gtest/gtest.h>

#include "bricks/file/file.h"
#include "bricks/strings/split.h"
#include "bricks/strings/join.h"
#include "bricks/time/chrono.h"

// #include "lib_c5t_dlib.h"
#include "lib_c5t_storage.h"
#include "lib_test_storage.h"

struct CallDefineTestStorageFields final {
  CallDefineTestStorageFields() { DefineTestStorageFields(); }
};
CallDefineTestStorageFields CallDefineTestStorageFields_impl;

inline std::string CurrentTestName() { return ::testing::UnitTest::GetInstance()->current_test_info()->name(); }

struct TestStorageDir final {
  std::string const dir;

  operator std::string const&() const { return dir; }

  TestStorageDir() : dir(InitDir()) {}

  static std::string InitDir() {
    // NOTE(dkorolev): I didn't find a quick way to get current binary dir and/or argv[0] from under `googletest`.
    std::vector<std::string> path = current::strings::Split(__FILE__, current::FileSystem::GetPathSeparator());
    path.pop_back();
#ifdef NDEBUG
    path.back() = ".current";
#else
    path.back() = ".current_debug";
#endif
    path.push_back(current::ToString(current::time::Now().count()));
    std::string const res = current::strings::Join(path, current::FileSystem::GetPathSeparator());
    std::string const dir = *__FILE__ == current::FileSystem::GetPathSeparator() ? "/" + res : res;
    current::FileSystem::MkDir(dir, current::FileSystem::MkDirParameters::Silent);
    return dir;
  }
};

#if 0
struct InitDLibOnce final {
  InitDLibOnce() {
    std::string const bin_path = []() {
      // NOTE(dkorolev): I didn't find a quick way to get current binary dir and/or argv[0] from under `googletest`.
      std::vector<std::string> path = current::strings::Split(__FILE__, current::FileSystem::GetPathSeparator());
      path.pop_back();
#ifdef NDEBUG
      path.back() = ".current";
#else
      path.back() = ".current_debug";
#endif
      std::string const res = current::strings::Join(path, current::FileSystem::GetPathSeparator());
      return *__FILE__ == current::FileSystem::GetPathSeparator() ? "/" + res : res;
    }();
    C5T_DLIB_SET_BASE_DIR(bin_path);
  }
};
#endif

TEST(StorageTest, FieldsList) {
  auto const storage_scope = C5T_STORAGE_CREATE_UNIQUE_INSANCE(current::Singleton<TestStorageDir>().dir);
  std::vector<std::string> v;
  C5T_STORAGE_LIST_FIELDS([&v](std::string const& s) { v.push_back(s); });
  EXPECT_EQ("kv1,kv2,kv3", current::strings::Join(v, ','));
}

TEST(StorageTest, NeedsStorage) {
  ASSERT_THROW(C5T_STORAGE(kv1), StorageNotInitializedException);
  ASSERT_THROW(C5T_STORAGE(kv1).Has("nope"), StorageNotInitializedException);
  auto const storage_scope = C5T_STORAGE_CREATE_UNIQUE_INSANCE(current::Singleton<TestStorageDir>().dir);
  EXPECT_FALSE(C5T_STORAGE(kv1).Has("nope"));
}

TEST(StorageTest, MapStringString) {
  auto const dir = current::Singleton<TestStorageDir>().dir + '/' + CurrentTestName();
  auto const storage_scope = C5T_STORAGE_CREATE_UNIQUE_INSANCE(dir);

  {
    ASSERT_FALSE(C5T_STORAGE(kv1).Has("k"));
    ASSERT_EQ("nope", C5T_STORAGE(kv1).GetOrDefault("k", "nope"));
    ASSERT_THROW(C5T_STORAGE(kv1).GetOrThrow("k"), StorageKeyNotFoundException);
    ASSERT_FALSE(Exists(C5T_STORAGE(kv1).Get("k")));
  }

  C5T_STORAGE(kv1).Set("k", "v");

  {
    ASSERT_TRUE(C5T_STORAGE(kv1).Has("k"));
    EXPECT_EQ("v", C5T_STORAGE(kv1).GetOrThrow("k"));

    ASSERT_EQ("v", C5T_STORAGE(kv1).GetOrDefault("k", "nope"));

    auto const o = C5T_STORAGE(kv1).Get("k");
    ASSERT_TRUE(Exists(o));
    EXPECT_EQ("v", Value(o));
  }
}

TEST(StorageTest, MapStringObject) {
  auto const dir = current::Singleton<TestStorageDir>().dir + '/' + CurrentTestName();
  auto const storage_scope = C5T_STORAGE_CREATE_UNIQUE_INSANCE(dir);

  {
    ASSERT_FALSE(C5T_STORAGE(kv2).Has("k"));
    ASSERT_THROW(C5T_STORAGE(kv2).GetOrThrow("k"), StorageKeyNotFoundException);
    ASSERT_FALSE(Exists(C5T_STORAGE(kv2).Get("k")));
  }

  C5T_STORAGE(kv2).Set("k", SomeJSON().SetFoo(42).SetBar("bar"));

  {
    ASSERT_TRUE(C5T_STORAGE(kv2).Has("k"));

    {
      auto const e = C5T_STORAGE(kv2).GetOrThrow("k");
      EXPECT_EQ(42, e.foo);
      ASSERT_TRUE(Exists(e.bar));
      EXPECT_EQ("bar", Value(e.bar));
    }

    {
      auto const o = C5T_STORAGE(kv2).Get("k");
      ASSERT_TRUE(Exists(o));
      auto const& e = Value(o);
      EXPECT_EQ(42, e.foo);
      ASSERT_TRUE(Exists(e.bar));
      EXPECT_EQ("bar", Value(e.bar));
    }
  }
}

TEST(StorageTest, MapPersists) {
  auto const dir1 = current::Singleton<TestStorageDir>().dir + '/' + CurrentTestName() + '1';
  auto const dir2 = current::Singleton<TestStorageDir>().dir + '/' + CurrentTestName() + '2';
  current::FileSystem::MkDir(dir1, current::FileSystem::MkDirParameters::Silent);

  {
    // Step 1/5: Create something and have it persisted.
    auto const storage_scope = C5T_STORAGE_CREATE_UNIQUE_INSANCE(dir1);
    EXPECT_FALSE(C5T_STORAGE(kv1).Has("k"));
    C5T_STORAGE(kv1).Set("k", "v");
  }

  {
    // Step 2/5: Restore from the persisted storage.
    auto const storage_scope = C5T_STORAGE_CREATE_UNIQUE_INSANCE(dir1);
    EXPECT_TRUE(C5T_STORAGE(kv1).Has("k"));
  }

  {
    // Step 3/5: But confirm that the freshly created storage from a different dir is empty.
    auto const storage_scope = C5T_STORAGE_CREATE_UNIQUE_INSANCE(dir2);
    EXPECT_FALSE(C5T_STORAGE(kv1).Has("k"));
  }

  {
    // Step 4/5: Restore from the persisted storage and delete the entry.
    auto const storage_scope = C5T_STORAGE_CREATE_UNIQUE_INSANCE(dir1);
    C5T_STORAGE(kv1).Del("k");
  }

  {
    // Step 5/5: Restore from the persisted storage.
    auto const storage_scope = C5T_STORAGE_CREATE_UNIQUE_INSANCE(dir1);
    EXPECT_FALSE(C5T_STORAGE(kv1).Has("k"));
  }
}

#if 0
TEST(StorageTest, InjectedFromDLib) {
  current::Singleton<InitDLibOnce>();

  EXPECT_EQ(42, C5T_DLIB_CALL("test_storage", [&](C5T_DLib& dlib) { return dlib.CallOrDefault<int()>("Smoke42"); }));
  EXPECT_EQ(0, C5T_DLIB_CALL("test_storage", [&](C5T_DLib& dlib) { return dlib.CallOrDefault<int()>("NonExistent"); }));

  EXPECT_EQ("OK", C5T_DLIB_CALL("test_storage", [&](C5T_DLib& dlib) {
              return dlib.CallOrDefault<std::string()>("SmokeOK");
            }));
  EXPECT_EQ("", C5T_DLIB_CALL("test_storage", [&](C5T_DLib& dlib) {
              return dlib.CallOrDefault<std::string()>("NonExistent");
            }));

  auto const dir = current::Singleton<TestStorageDir>().dir + '/' + CurrentTestName();
  current::FileSystem::MkDir(dir, current::FileSystem::MkDirParameters::Silent);
  auto const storage_scope = C5T_STORAGE_CREATE_UNIQUE_INSANCE(dir);

  IStorage istorage(C5T_STORAGE_INSTANCE());

  Optional<std::string> const storage_fields = C5T_DLIB_CALL("test_storage", [&](C5T_DLib& dlib) {
    return dlib.CallReturningOptional<std::string(IDLib&)>("StorageFields", istorage);
  });

  EXPECT_TRUE(Exists(storage_fields));
  EXPECT_EQ("kv1,kv2,kv3", Value(storage_fields));

  EXPECT_FALSE(C5T_STORAGE(kv1).Has("k"));
  ASSERT_THROW(C5T_STORAGE(kv1).GetOrThrow("k"), StorageKeyNotFoundException);

  C5T_DLIB_CALL("test_storage", [&](C5T_DLib& dlib) {
    dlib.CallVoid<void(IDLib&, std::string const& key, std::string const& value)>("TestSet", istorage, "k", "v1");
  });

  EXPECT_TRUE(C5T_STORAGE(kv1).Has("k"));
  EXPECT_EQ("v1", C5T_STORAGE(kv1).GetOrThrow("k"));

  C5T_DLIB_USE("test_storage", [&](C5T_DLib& dlib) {
    dlib.CallVoid<void(IDLib&, std::string const& key, std::string const& value)>("TestSet", istorage, "k", "v2");
  });

  EXPECT_TRUE(C5T_STORAGE(kv1).Has("k"));
  EXPECT_EQ("v2", C5T_STORAGE(kv1).GetOrThrow("k"));

  C5T_DLIB_USE("test_storage",
               [&](C5T_DLib& dlib) { dlib.CallVoid<void(IDLib&, std::string const& key)>("TestDel", istorage, "k"); });

  EXPECT_FALSE(C5T_STORAGE(kv1).Has("k"));
  ASSERT_THROW(C5T_STORAGE(kv1).GetOrThrow("k"), StorageKeyNotFoundException);
}
#endif

TEST(StorageTest, ThrowsOnDeclaredAndNotDefinedField) {
  ASSERT_THROW(C5T_STORAGE(kv_declared_but_not_defined), StorageNotInitializedException);
  ASSERT_THROW(C5T_STORAGE(kv_declared_but_not_defined).Has("nope"), StorageNotInitializedException);
  auto const storage_scope = C5T_STORAGE_CREATE_UNIQUE_INSANCE(current::Singleton<TestStorageDir>().dir);
  ASSERT_THROW(C5T_STORAGE(kv_declared_but_not_defined), StorageFieldDeclaredAndNotDefinedException);
  ASSERT_THROW(C5T_STORAGE(kv_declared_but_not_defined).Has("nope"), StorageFieldDeclaredAndNotDefinedException);
}

// TO TEST:
// [x] do not use the global storage singleton, inject one per test
// [x] persist restore trivial
// [x] persist restore recovering
// [ ] persist move old files around, to not scan through them unnecesarily
// [ ] persist evolve, use `JSONFormat::Minimalistic`
// [ ] persist set logger and errors on failing to evolve
// [x] delete
// [x] injected storage
