/**
 *    Copyright (C) 2014 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#pragma once

#include <map>

#include "mongo/db/service_context.h"

namespace mongo {

class Client;
class StorageEngineLockFile;

//ServiceContextMongoD->ServiceContext(包含ServiceContext成员)
//ServiceEntryPointMongod->ServiceEntryPointImpl->ServiceEntryPoint

//_initAndListen中会构造使用该类  
class ServiceContextMongoD final : public ServiceContext {
public:
    //下面的FactoryMap _storageFactories;使用
    using FactoryMap = std::map<std::string, const StorageEngine::Factory*>;

    ServiceContextMongoD();

    ~ServiceContextMongoD();

    StorageEngine* getGlobalStorageEngine() override;

    void createLockFile();
    //初始化存储引擎
    void initializeGlobalStorageEngine() override;

    void shutdownGlobalStorageEngineCleanly() override;

    //注册name factory到_storageFactories
    void registerStorageEngine(const std::string& name,
                               const StorageEngine::Factory* factory) override;

    //name是否在_storageFactories有注册
    bool isRegisteredStorageEngine(const std::string& name) override;

    //遍历下面的_storageFactories成员 map表用的
    StorageFactoriesIterator* makeStorageFactoriesIterator() override;

private:
    ////生成一个OperationContext类
    std::unique_ptr<OperationContext> _newOpCtx(Client* client, unsigned opId) override;

    //createLockFile中创建lockfile ServiceContextMongoD::initializeGlobalStorageEngine()中close
    std::unique_ptr<StorageEngineLockFile> _lockFile;

    // logically owned here, but never deleted by anyone.
    //逻辑存储引擎  MongoDB只有一个存储引擎，叫做MMAP，MongoDB3.0的推出使得MongoDB有了两个引擎：MMAPv1和WiredTiger。
    //ServiceContextMongoD::initializeGlobalStorageEngine中赋值
    StorageEngine* _storageEngine = nullptr; //当前用的存储引擎

    // All possible storage engines are registered here through MONGO_INIT.
    //所有的存储引起都注册到这里 registerStorageEngine
    FactoryMap _storageFactories;
};

//上面的ServiceContextMongoD:makeStorageFactoriesIterator 使用到该类做遍历
class StorageFactoriesIteratorMongoD final : public StorageFactoriesIterator {
public:
    typedef ServiceContextMongoD::FactoryMap::const_iterator FactoryMapIterator;

    StorageFactoriesIteratorMongoD(const FactoryMapIterator& begin, const FactoryMapIterator& end);

    bool more() const override;
    const StorageEngine::Factory* next() override;

private:
    FactoryMapIterator _curr;
    FactoryMapIterator _end;
};

}  // namespace mongo

