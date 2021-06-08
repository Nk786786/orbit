// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDateTime>
#include <chrono>
#include <filesystem>
#include <thread>

#include "CaptureFileInfo/Manager.h"
#include "OrbitBase/ExecutablePath.h"

namespace orbit_capture_file_info {

constexpr const char* kOrgName = "The Orbit Authors";

TEST(CaptureFileInfoManager, Clear) {
  QCoreApplication::setOrganizationName(kOrgName);
  QCoreApplication::setApplicationName("CaptureFileInfo.Manager.Clear");

  Manager manager;

  manager.Clear();
  EXPECT_TRUE(manager.GetCaptureFileInfos().empty());

  manager.AddOrTouchCaptureFile("test/path1");
  EXPECT_FALSE(manager.GetCaptureFileInfos().empty());

  manager.Clear();
  EXPECT_TRUE(manager.GetCaptureFileInfos().empty());

  manager.AddOrTouchCaptureFile("test/path1");
  EXPECT_FALSE(manager.GetCaptureFileInfos().empty());
  manager.AddOrTouchCaptureFile("test/path2");
  EXPECT_FALSE(manager.GetCaptureFileInfos().empty());

  manager.Clear();
  EXPECT_TRUE(manager.GetCaptureFileInfos().empty());
}

TEST(CaptureFileInfoManager, AddOrTouchCaptureFile) {
  QCoreApplication::setOrganizationName(kOrgName);
  QCoreApplication::setApplicationName("CaptureFileInfo.Manager.AddOrTouchCaptureFile");

  Manager manager;
  manager.Clear();
  ASSERT_TRUE(manager.GetCaptureFileInfos().empty());

  // Add 1st file
  const QString path1 = "path/to/file1";
  manager.AddOrTouchCaptureFile(path1);
  ASSERT_EQ(manager.GetCaptureFileInfos().size(), 1);

  const CaptureFileInfo& capture_file_info_1 = manager.GetCaptureFileInfos()[0];
  EXPECT_EQ(capture_file_info_1.FilePath(), path1);
  // last used was before (or at the same time) than now_time_stamp.
  const QDateTime now_time_stamp = QDateTime::currentDateTime();
  EXPECT_LE(capture_file_info_1.LastUsed(), now_time_stamp);

  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  // Touch 1st file
  manager.AddOrTouchCaptureFile(path1);
  ASSERT_EQ(manager.GetCaptureFileInfos().size(), 1);

  // last used was after now_time_stamp
  EXPECT_GT(capture_file_info_1.LastUsed(), now_time_stamp);

  // Add 2nd file
  const QString path2 = "path/to/file2";
  manager.AddOrTouchCaptureFile(path2);
  ASSERT_EQ(manager.GetCaptureFileInfos().size(), 2);
  const CaptureFileInfo& capture_file_info_2 = manager.GetCaptureFileInfos()[1];
  EXPECT_EQ(capture_file_info_2.FilePath(), path2);

  // clean up
  manager.Clear();
}

TEST(CaptureFileInfoManager, PurgeNonExistingFiles) {
  QCoreApplication::setOrganizationName(kOrgName);
  QCoreApplication::setApplicationName("CaptureFileInfo.Manager.PurgeNonExistingFiles");

  Manager manager;
  manager.Clear();
  EXPECT_TRUE(manager.GetCaptureFileInfos().empty());

  manager.AddOrTouchCaptureFile("non/existing/path");
  EXPECT_FALSE(manager.GetCaptureFileInfos().empty());

  manager.PurgeNonExistingFiles();
  EXPECT_TRUE(manager.GetCaptureFileInfos().empty());

  const std::filesystem::path existing_file =
      orbit_base::GetExecutableDir() / "testdata" / "test_file.txt";
  manager.AddOrTouchCaptureFile(QString::fromStdString(existing_file.string()));
  EXPECT_FALSE(manager.GetCaptureFileInfos().empty());

  manager.PurgeNonExistingFiles();
  EXPECT_FALSE(manager.GetCaptureFileInfos().empty());

  // clean up
  manager.Clear();
}

TEST(CaptureFileInfoManager, Persistency) {
  QCoreApplication::setOrganizationName(kOrgName);
  QCoreApplication::setApplicationName("CaptureFileInfo.Manager.Persistency");

  {  // clean setup
    Manager manager;
    manager.Clear();
  }

  {
    Manager manager;
    EXPECT_TRUE(manager.GetCaptureFileInfos().empty());
  }

  const std::filesystem::path existing_file =
      orbit_base::GetExecutableDir() / "testdata" / "test_file.txt";
  {
    Manager manager;
    manager.AddOrTouchCaptureFile(QString::fromStdString(existing_file.string()));
    EXPECT_EQ(manager.GetCaptureFileInfos().size(), 1);
  }

  {
    Manager manager;
    ASSERT_EQ(manager.GetCaptureFileInfos().size(), 1);
    const CaptureFileInfo capture_file_info = manager.GetCaptureFileInfos()[0];

    std::filesystem::path saved_path = capture_file_info.FilePath().toStdString();

    EXPECT_EQ(saved_path, existing_file);
  }

  {  // clean up
    Manager manager;
    manager.Clear();
  }
}

}  // namespace orbit_capture_file_info