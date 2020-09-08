/**
 *    Copyright (C) 2013 10gen Inc.
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
 *    must comply with the GNU Affero General Public License in all respects
 *    for all of the code used other than as permitted herein. If you modify
 *    file(s) with this exception, you may extend this exception to your
 *    version of the file(s), but you are not obligated to do so. If you do not
 *    wish to do so, delete this exception statement from your version. If you
 *    delete this exception statement from all source files in the program,
 *    then also delete it in the license file.
 */

#include "mongo/platform/basic.h"

#include "mongo/s/shard_key_pattern.h"

#include <vector>

#include "mongo/db/field_ref.h"
#include "mongo/db/field_ref_set.h"
#include "mongo/db/hasher.h"
#include "mongo/db/index_names.h"
#include "mongo/db/matcher/extensions_callback_noop.h"
#include "mongo/db/query/canonical_query.h"
#include "mongo/db/update/path_support.h"
#include "mongo/util/mongoutils/str.h"
#include "mongo/util/transitional_tools_do_not_use/vector_spooling.h"

namespace mongo {

using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

using pathsupport::EqualityMatches;

const int ShardKeyPattern::kMaxShardKeySizeBytes = 512;
const unsigned int ShardKeyPattern::kMaxFlattenedInCombinations = 4000000;

namespace {

bool isHashedPatternEl(const BSONElement& el) {
    return el.type() == String && el.String() == IndexNames::HASHED;
}

/**
 * Currently the allowable shard keys are either:
 * i) a hashed single field, e.g. { a : "hashed" }, or
 * ii) a compound list of ascending, potentially-nested field paths, e.g. { a : 1 , b.c : 1 }
 */ //解析出索引信息，如{ a : "hashed" }  { a : 1 , b.c : 1 }等
std::vector<std::unique_ptr<FieldRef>> 
parseShardKeyPattern(const BSONObj& keyPattern) {
    std::vector<std::unique_ptr<FieldRef>> parsedPaths;

    for (const auto& patternEl : keyPattern) {
        auto newFieldRef(stdx::make_unique<FieldRef>(patternEl.fieldNameStringData()));

        // Empty path
        if (newFieldRef->numParts() == 0)
            return {};

        // Extra "." in path?
        if (newFieldRef->dottedField() != patternEl.fieldNameStringData())
            return {};

        // Empty parts of the path, ".."?
        for (size_t i = 0; i < newFieldRef->numParts(); ++i) {
            if (newFieldRef->getPart(i).empty())
                return {};
        }

        // Numeric and ascending (1.0), or "hashed" and single field
        if (!patternEl.isNumber()) {
            if (keyPattern.nFields() != 1 || !isHashedPatternEl(patternEl))
                return {};
        } else if (patternEl.numberInt() != 1) {
            return {};
        }

        parsedPaths.emplace_back(std::move(newFieldRef));
    }

    return parsedPaths;
}

bool isShardKeyElement(const BSONElement& element, bool allowRegex) {
    if (element.eoo() || element.type() == Array)
        return false;

    // TODO: Disallow regex all the time
    if (!allowRegex && element.type() == RegEx)
        return false;

    if (element.type() == Object && !element.embeddedObject().storageValidEmbedded().isOK())
        return false;

    return true;
}

}  // namespace

//分片片建最大长度限制检查
Status ShardKeyPattern::checkShardKeySize(const BSONObj& shardKey) {
    if (shardKey.objsize() <= kMaxShardKeySizeBytes)
        return Status::OK();

    return {ErrorCodes::ShardKeyTooBig,
            str::stream() << "shard keys must be less than " << kMaxShardKeySizeBytes
                          << " bytes, but key "
                          << shardKey
                          << " is "
                          << shardKey.objsize()
                          << " bytes"};
}

ShardKeyPattern::ShardKeyPattern(const BSONObj& keyPattern)
	//解析出索引信息，如{ a : "hashed" }  { a : 1 , b.c : 1 }等
    : _keyPatternPaths(parseShardKeyPattern(keyPattern)),
    //原始BSONObj
      _keyPattern(_keyPatternPaths.empty() ? BSONObj() : keyPattern),
      //是否有_id索引
      _hasId(keyPattern.hasField("_id"_sd)) {}

ShardKeyPattern::ShardKeyPattern(const KeyPattern& keyPattern)
    : ShardKeyPattern(keyPattern.toBSON()) {}

bool ShardKeyPattern::isValid() const {
    return !_keyPattern.toBSON().isEmpty();
}

//是否hash索引
bool ShardKeyPattern::isHashedPattern() const {
    return isHashedPatternEl(_keyPattern.toBSON().firstElement());
}

//从ShardKeyPattern中获取_keyPattern成员
const KeyPattern& ShardKeyPattern::getKeyPattern() const {
    return _keyPattern;
}

//获取_keyPatternPaths信息
const std::vector<std::unique_ptr<FieldRef>>& ShardKeyPattern::getKeyPatternFields() const {
    return _keyPatternPaths;
}

//获取_keyPattern对应toBSON信息
const BSONObj& ShardKeyPattern::toBSON() const {
    return _keyPattern.toBSON();
}

//ShardKeyPattern对应的string信息
string ShardKeyPattern::toString() const {
    return toBSON().toString();
}

//shardKey是否和_keyPattern内容一致，如果一致可以判断为是分片片建
bool ShardKeyPattern::isShardKey(const BSONObj& shardKey) const {
    // Shard keys are always of the form: { 'nested.path' : value, 'nested.path2' : value }

    if (!isValid())
        return false;

    const auto& keyPatternBSON = _keyPattern.toBSON();

    for (const auto& patternEl : keyPatternBSON) {
        BSONElement keyEl = shardKey[patternEl.fieldNameStringData()];

        if (!isShardKeyElement(keyEl, true))
            return false;
    }

    return shardKey.nFields() == keyPatternBSON.nFields();
}

BSONObj ShardKeyPattern::normalizeShardKey(const BSONObj& shardKey) const {
    // Shard keys are always of the form: { 'nested.path' : value, 'nested.path2' : value }
    // and in the same order as the key pattern

    if (!isValid())
        return BSONObj();

    // We want to return an empty key if users pass us something that's not a shard key
    //shardKey不正确，直接返回空的BSONObj
    if (shardKey.nFields() > _keyPattern.toBSON().nFields())
        return BSONObj();

    BSONObjBuilder keyBuilder;
    BSONObjIterator patternIt(_keyPattern.toBSON());
    while (patternIt.more()) {
        BSONElement patternEl = patternIt.next();

        BSONElement keyEl = shardKey[patternEl.fieldNameStringData()];

        if (!isShardKeyElement(keyEl, true))
            return BSONObj();

        keyBuilder.appendAs(keyEl, patternEl.fieldName());
    }

    dassert(isShardKey(keyBuilder.asTempObj()));
    return keyBuilder.obj();
}

static BSONElement extractKeyElementFromMatchable(const MatchableDocument& matchable,
                                                  StringData pathStr) {
    ElementPath path;
    path.init(pathStr).transitional_ignore();
    path.setTraverseNonleafArrays(false);
    path.setTraverseLeafArray(false);

    MatchableDocument::IteratorHolder matchIt(&matchable, &path);
    if (!matchIt->more())
        return BSONElement();

    BSONElement matchEl = matchIt->next().element();
    // We shouldn't have more than one element - we don't expand arrays
    dassert(!matchIt->more());

    return matchEl;
}

/**
 * Given a MatchableDocument, extracts the shard key corresponding to the key pattern.
 * For each path in the shard key pattern, extracts a value from the matchable document.
 *
 * Paths to shard key fields must not contain arrays at any level, and shard keys may not
 * be array fields, undefined, or non-storable sub-documents.  If the shard key pattern is
 * a hashed key pattern, this method performs the hashing.
 *
 * If a shard key cannot be extracted, returns an empty BSONObj().
 *
 * Examples:
 *	If 'this' KeyPattern is { a  : 1 }
 *	 { a: "hi" , b : 4} --> returns { a : "hi" }， 到a="hi"这个分片
 *	 { c : 4 , a : 2 } -->	returns { a : 2 }， 到a=2这个分片
 *	 { b : 2 }	-> returns {}，到所有分片
 *	 { a : [1,2] } -> returns {}，到所有分片，因为这里是一个范围
 *	If 'this' KeyPattern is { a  : "hashed" }
 *	 { a: 1 } --> returns { a : NumberLong("5902408780260971510")  } 
 *	If 'this' KeyPattern is { 'a.b' : 1 }
 *	 { a : { b : "hi" } } --> returns { 'a.b' : "hi" }
 *	 { a : [{ b : "hi" }] } --> returns {}
 */

//从matchable中解析匹配shardkey对应的信息  ShardKeyPattern::extractShardKeyFromDoc中调用
BSONObj ShardKeyPattern::extractShardKeyFromMatchable(const MatchableDocument& matchable) const {
    if (!isValid())
        return BSONObj();

    BSONObjBuilder keyBuilder;

    BSONObjIterator patternIt(_keyPattern.toBSON());
    while (patternIt.more()) {
        BSONElement patternEl = patternIt.next();
        BSONElement matchEl =
            extractKeyElementFromMatchable(matchable, patternEl.fieldNameStringData());

        if (!isShardKeyElement(matchEl, true))
            return BSONObj();

        if (isHashedPatternEl(patternEl)) {
            keyBuilder.append(
                patternEl.fieldName(),
                BSONElementHasher::hash64(matchEl, BSONElementHasher::DEFAULT_HASH_SEED));
        } else {
            // NOTE: The matched element may *not* have the same field name as the path -
            // index keys don't contain field names, for example
            keyBuilder.appendAs(matchEl, patternEl.fieldName());
        }
    }

    dassert(isShardKey(keyBuilder.asTempObj()));
    return keyBuilder.obj();
}

//从doc中解析shardkey对应的值  
BSONObj ShardKeyPattern::extractShardKeyFromDoc(const BSONObj& doc) const {
    BSONMatchableDocument matchable(doc);
    return extractShardKeyFromMatchable(matchable);
}


//ShardKeyPattern::extractShardKeyFromQuery调用
static BSONElement findEqualityElement(const EqualityMatches& equalities, const FieldRef& path) {
    int parentPathPart;
    const BSONElement parentEl =
        pathsupport::findParentEqualityElement(equalities, path, &parentPathPart);

    if (parentPathPart == static_cast<int>(path.numParts()))
        return parentEl;

    if (parentEl.type() != Object)
        return BSONElement();

    StringData suffixStr = path.dottedSubstring(parentPathPart, path.numParts());
    BSONMatchableDocument matchable(parentEl.Obj());
    return extractKeyElementFromMatchable(matchable, suffixStr);
}


/**
 * Given a simple BSON query, extracts the shard key corresponding to the key pattern
 * from equality matches in the query.	The query expression *must not* be a complex query
 * with sorts or other attributes.
 *
 * Logically, the equalities in the BSON query can be serialized into a BSON document and
 * then a shard key is extracted from this equality document.
 *
 * NOTE: BSON queries and BSON documents look similar but are different languages.	Use the
 * correct shard key extraction function.
 *
 * Returns !OK status if the query cannot be parsed.  Returns an empty BSONObj() if there is
 * no shard key found in the query equalities.
 *
 * Examples:
 *	If the key pattern is { a : 1 }
 *	 { a : "hi", b : 4 } --> returns { a : "hi" }
 *	 { a : { $eq : "hi" }, b : 4 } --> returns { a : "hi" }
 *	 { $and : [{a : { $eq : "hi" }}, { b : 4 }] } --> returns { a : "hi" }
 *	If the key pattern is { 'a.b' : 1 }
 *	 { a : { b : "hi" } } --> returns { 'a.b' : "hi" }
 *	 { 'a.b' : "hi" } --> returns { 'a.b' : "hi" }
 *	 { a : { b : { $eq : "hi" } } } --> returns {} because the query language treats this as
 *												   a : { $eq : { b : ... } }
 */

////从basicQuery中提取shard key信息
StatusWith<BSONObj> ShardKeyPattern::extractShardKeyFromQuery(OperationContext* opCtx,
                                                              const BSONObj& basicQuery) const {
    if (!isValid())
        return StatusWith<BSONObj>(BSONObj());

    auto qr = stdx::make_unique<QueryRequest>(NamespaceString(""));
    qr->setFilter(basicQuery);

    const boost::intrusive_ptr<ExpressionContext> expCtx;
    auto statusWithCQ =
        CanonicalQuery::canonicalize(opCtx,
                                     std::move(qr),
                                     expCtx,
                                     ExtensionsCallbackNoop(),
                                     MatchExpressionParser::kAllowAllSpecialFeatures);
    if (!statusWithCQ.isOK()) {
        return StatusWith<BSONObj>(statusWithCQ.getStatus());
    }
    unique_ptr<CanonicalQuery> query = std::move(statusWithCQ.getValue());

    return extractShardKeyFromQuery(*query);
}

//从CanonicalQuery中提取shard key信息  ShardKeyPattern::extractShardKeyFromQuery调用
BSONObj ShardKeyPattern::extractShardKeyFromQuery(const CanonicalQuery& query) const {
    if (!isValid())
        return BSONObj();

    // Extract equalities from query.
    EqualityMatches equalities;
    // TODO: Build the path set initially?
    FieldRefSet keyPatternPathSet(transitional_tools_do_not_use::unspool_vector(_keyPatternPaths));
    // We only care about extracting the full key pattern paths - if they don't exist (or are
    // conflicting), we don't contain the shard key.
    Status eqStatus =
        pathsupport::extractFullEqualityMatches(*query.root(), keyPatternPathSet, &equalities);
    // NOTE: Failure to extract equality matches just means we return no shard key - it's not
    // an error we propagate
    if (!eqStatus.isOK())
        return BSONObj();

    // Extract key from equalities
    // NOTE: The method below is equivalent to constructing a BSONObj and running
    // extractShardKeyFromMatchable, but doesn't require creating the doc.

    BSONObjBuilder keyBuilder;
    // Iterate the parsed paths to avoid re-parsing
    for (auto it = _keyPatternPaths.begin(); it != _keyPatternPaths.end(); ++it) {
        const FieldRef& patternPath = **it;
        BSONElement equalEl = findEqualityElement(equalities, patternPath);

        if (!isShardKeyElement(equalEl, false))
            return BSONObj();

        if (isHashedPattern()) {
            keyBuilder.append(
                patternPath.dottedField(),
                BSONElementHasher::hash64(equalEl, BSONElementHasher::DEFAULT_HASH_SEED));
        } else {
            // NOTE: The equal element may *not* have the same field name as the path -
            // nested $and, $eq, for example
            keyBuilder.appendAs(equalEl, patternPath.dottedField());
        }
    }

    dassert(isShardKey(keyBuilder.asTempObj()));
    return keyBuilder.obj();
}

/**
 * Returns true if the shard key pattern can ensure that the unique index pattern is
 * respected across all shards.
 *
 * Primarily this just checks whether the shard key pattern field names are equal to or a
 * prefix of the unique index pattern field names.	Since documents with the same fields in
 * the shard key pattern are guaranteed to go to the same shard, and all documents must
 * contain the full shard key, a unique index with a shard key pattern prefix can be sure
 * when resolving duplicates that documents on other shards will have different shard keys,
 * and so are not duplicates.
 *
 * Hashed shard key patterns are similar to ordinary patterns in that they guarantee similar
 * shard keys go to the same shard.
 *
 * Examples:
 *	   shard key {a : 1} is compatible with a unique index on {_id : 1}
 *	   shard key {a : 1} is compatible with a unique index on {a : 1 , b : 1}
 *	   shard key {a : 1} is compatible with a unique index on {a : -1 , b : 1 }
 *	   shard key {a : "hashed"} is compatible with a unique index on {a : 1}
 *	   shard key {a : 1} is not compatible with a unique index on {b : 1}
 *	   shard key {a : "hashed" , b : 1 } is not compatible with unique index on { b : 1 }
 *
 * All unique index patterns starting with _id are assumed to be enforceable by the fact
 * that _ids must be unique, and so all unique _id prefixed indexes are compatible with
 * any shard key pattern.
 *
 * NOTE: We assume 'uniqueIndexPattern' is a valid unique index pattern - a pattern like
 * { k : "hashed" } is not capable of being a unique index and is an invalid argument to
 * this method.
 */

//分片模式创建唯一索引的时候，唯一索引最左字段必须包含片建  参考checkUniqueIndexConstraints
bool ShardKeyPattern::isUniqueIndexCompatible(const BSONObj& uniqueIndexPattern) const {
    dassert(!KeyPattern::isHashedKeyPattern(uniqueIndexPattern));

    if (!uniqueIndexPattern.isEmpty() &&
        string("_id") == uniqueIndexPattern.firstElementFieldName()) {
        return true;
    }

    return _keyPattern.toBSON().isFieldNamePrefixOf(uniqueIndexPattern);
}

/**
 * Return an ordered list of bounds generated using this KeyPattern and the
 * bounds from the IndexBounds.  This function is used in sharding to
 * determine where to route queries according to the shard key pattern.
 *
 * Examples:
 *
 * Key { a: 1 }, Bounds a: [0] => { a: 0 } -> { a: 0 }
 * Key { a: 1 }, Bounds a: [2, 3) => { a: 2 } -> { a: 3 }  // bound inclusion ignored.
 *
 * The bounds returned by this function may be a superset of those defined
 * by the constraints.	For instance, if this KeyPattern is {a : 1, b: 1}
 * Bounds: { a : {$in : [1,2]} , b : {$in : [3,4,5]} }
 *		   => {a : 1 , b : 3} -> {a : 1 , b : 5}, {a : 2 , b : 3} -> {a : 2 , b : 5}
 *
 * If the IndexBounds are not defined for all the fields in this keypattern, which
 * means some fields are unsatisfied, an empty BoundList could return.
 *
 */
/*
// Transforms bounds for each shard key field into full shard key ranges
// for example :
//	 Key { a : 1, b : 1 }  索引
//	 Query { a : { $gte : 1, $lt : 2 },
//			  b : { $gte : 3, $lt : 4 } }  查询条件
//	 Bounds { a : [1, 2), b : [3, 4) }       转换为IndexBounds类型
//	 => Ranges { a : 1, b : 3 } => { a : 2, b : 4 } 转换为BoundList类型

typedef std::vector<std::pair< BSONObj, BSONObj >> BoundList;
*/ 
//ChunkManager::getShardIdsForQuery调用,把indexBounds转换为
BoundList ShardKeyPattern::flattenBounds(const IndexBounds& indexBounds) const {
    invariant(indexBounds.fields.size() == (size_t)_keyPattern.toBSON().nFields());

    // If any field is unsatisfied, return empty bound list.
    for (vector<OrderedIntervalList>::const_iterator it = indexBounds.fields.begin();
         it != indexBounds.fields.end();
         it++) {
        if (it->intervals.size() == 0) {
            return BoundList();
        }
    }

    // To construct our bounds we will generate intervals based on bounds for the first field, then
    // compound intervals based on constraints for the first 2 fields, then compound intervals for
    // the first 3 fields, etc.
    //
    // As we loop through the fields, we start generating new intervals that will later get extended
    // in another iteration of the loop. We define these partially constructed intervals using pairs
    // of BSONObjBuilders (shared_ptrs, since after one iteration of the loop they still must exist
    // outside their scope).
    typedef vector<std::pair<std::shared_ptr<BSONObjBuilder>, std::shared_ptr<BSONObjBuilder>>>
        BoundBuilders;

    BoundBuilders builders;
    builders.emplace_back(shared_ptr<BSONObjBuilder>(new BSONObjBuilder()),
                          shared_ptr<BSONObjBuilder>(new BSONObjBuilder()));
    BSONObjIterator keyIter(_keyPattern.toBSON());
    // until equalityOnly is false, we are just dealing with equality (no range or $in queries).
    bool equalityOnly = true;

    for (size_t i = 0; i < indexBounds.fields.size(); i++) {
        BSONElement e = keyIter.next();

        StringData fieldName = e.fieldNameStringData();

        // get the relevant intervals for this field, but we may have to transform the
        // list of what's relevant according to the expression for this field
        const OrderedIntervalList& oil = indexBounds.fields[i];
        const vector<Interval>& intervals = oil.intervals;

        if (equalityOnly) {
            if (intervals.size() == 1 && intervals.front().isPoint()) {
                // this field is only a single point-interval
                BoundBuilders::const_iterator j;
                for (j = builders.begin(); j != builders.end(); ++j) {
                    j->first->appendAs(intervals.front().start, fieldName);
                    j->second->appendAs(intervals.front().end, fieldName);
                }
            } else {
                // This clause is the first to generate more than a single point.
                // We only execute this clause once. After that, we simplify the bound
                // extensions to prevent combinatorial explosion.
                equalityOnly = false;

                BoundBuilders newBuilders;

                for (BoundBuilders::const_iterator it = builders.begin(); it != builders.end();
                     ++it) {
                    BSONObj first = it->first->obj();
                    BSONObj second = it->second->obj();

                    for (vector<Interval>::const_iterator interval = intervals.begin();
                         interval != intervals.end();
                         ++interval) {
                        uassert(17439,
                                "combinatorial limit of $in partitioning of results exceeded",
                                newBuilders.size() < kMaxFlattenedInCombinations);
                        newBuilders.emplace_back(shared_ptr<BSONObjBuilder>(new BSONObjBuilder()),
                                                 shared_ptr<BSONObjBuilder>(new BSONObjBuilder()));
                        newBuilders.back().first->appendElements(first);
                        newBuilders.back().second->appendElements(second);
                        newBuilders.back().first->appendAs(interval->start, fieldName);
                        newBuilders.back().second->appendAs(interval->end, fieldName);
                    }
                }
                builders = newBuilders;
            }
        } else {
            // if we've already generated a range or multiple point-intervals
            // just extend what we've generated with min/max bounds for this field
            BoundBuilders::const_iterator j;
            for (j = builders.begin(); j != builders.end(); ++j) {
                j->first->appendAs(intervals.front().start, fieldName);
                j->second->appendAs(intervals.back().end, fieldName);
            }
        }
    }

    BoundList ret;
    for (BoundBuilders::const_iterator i = builders.begin(); i != builders.end(); ++i)
        ret.emplace_back(i->first->obj(), i->second->obj());

    return ret;
}

}  // namespace mongo
