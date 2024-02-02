/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

// This file is part of <ph/va.h>. Do NOT include it directly from your source code. Include <ph/va.h> instead.

namespace ph {
namespace va {

// ---------------------------------------------------------------------------------------------------------------------
/// The async timestamp query helper class. Each class can hold up to 16 timestamp queries.
class AsyncTimestamps {
public:
    typedef uint64_t QueryID;

    static constexpr QueryID INVALID_ID = 0;

    struct ConstructParameters {
        VulkanSubmissionProxy & vsp;
        const char *            name = nullptr; /// optional string to name the query pool
    };

    struct QueryResult {
        const char * name       = nullptr;
        uint64_t     durationNs = 0; // duration in nanoseconds
    };

    struct ScopedQuery {
        AsyncTimestamps & q;
        uint64_t          id;
        ScopedQuery(AsyncTimestamps & q_, VkCommandBuffer cb, const char * name): q(q_), id(q.begin(cb, name)) {}
        ScopedQuery(AsyncTimestamps * q_, VkCommandBuffer cb, const char * name): q(*q_), id(q_ ? q_->begin(cb, name) : 0) {}
        ~ScopedQuery() { end(); }
        void end() {
            if (id != 0) {
                q.end(id);
                id = 0;
            }
        }
    };

    AsyncTimestamps(const ConstructParameters &);

    ~AsyncTimestamps();

    QueryID  begin(VkCommandBuffer, const char * name);
    void     end();                 // end query by id
    void     end(QueryID);          // end query by id
    uint64_t report(QueryID) const; // get the latest query result

    // Call this at least once per frame to refresh query result. must be called out side of a render pass.
    void refresh(VkCommandBuffer cb);

    /// Get all query results
    std::vector<QueryResult> reportAll() const;

private:
    class Impl;
    Impl * _impl = nullptr;
};

} // namespace va
} // namespace ph