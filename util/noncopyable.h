/*
 * =====================================================================================
 *
 *       Filename:  noncopyable.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2018年03月02日 16时53分27秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */

#pragma once

namespace cortono::util
{
    class noncopyable
    {
        public:
            noncopyable() {}

            noncopyable(const noncopyable&) = delete;
            noncopyable& operator=(const noncopyable&) = delete;
    };
}
