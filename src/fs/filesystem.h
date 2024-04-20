/******************************************************************************
 *
 *  Project:	ConverterPIX @ Core
 *  File:		/fs/filesystem.h
 *
 *		  _____                          _            _____ _______   __
 *		 / ____|                        | |          |  __ \_   _\ \ / /
 *		| |     ___  _ ____   _____ _ __| |_ ___ _ __| |__) || |  \ V /
 *		| |    / _ \| '_ \ \ / / _ \ '__| __/ _ \ '__|  ___/ | |   > <
 *		| |___| (_) | | | \ V /  __/ |  | ||  __/ |  | |    _| |_ / . \
 *		 \_____\___/|_| |_|\_/ \___|_|   \__\___|_|  |_|   |_____/_/ \_\
 *
 *
 *  Copyright (C) 2017 Michal Wojtowicz.
 *  All rights reserved.
 *
 *   This software is ditributed WITHOUT ANY WARRANTY; without even
 *   the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *   PURPOSE. See the copyright file for more information.
 *
 *****************************************************************************/

#pragma once

#include "utils/token.h"
#include "structs/fs.h"

class MetaStat;

class FileSystem
{
public:
	class Entry;

	enum FsOpenMode
	{
		OpenModeNone = 0
	};
	static constexpr FsOpenMode read =		( FsOpenMode )( 1 << 0 );
	static constexpr FsOpenMode write =		( FsOpenMode )( 1 << 1 );
	static constexpr FsOpenMode append =	( FsOpenMode )( 1 << 2 );
	static constexpr FsOpenMode update =	( FsOpenMode )( 1 << 3 );
	static constexpr FsOpenMode binary =	( FsOpenMode )( 1 << 4 );

public:
	FileSystem();
	FileSystem( const FileSystem & ) = delete;
	FileSystem( FileSystem && ) = delete;
	virtual ~FileSystem();

	FileSystem &operator=( FileSystem & ) = delete;
	FileSystem &operator=( FileSystem && ) = delete;

	/**
	 * Returns root of the filesystem ended with slash. Unless root is empty, then it returns empty string
	 */
	virtual String root() const = 0;

	virtual String name() const = 0;

	/**
	 * Out param outFileExists indicates if file exists. Important: implementation is not obligated to set false.
	 */
	virtual UniquePtr<File> open( const String &filePath, FsOpenMode mode, bool *outFileExists = nullptr ) = 0;

	virtual bool remove( const String &filePath ) = 0;

	virtual bool mkdir( const String &directory ) = 0;
	virtual bool rmdir( const String &directory ) = 0;

	virtual bool exists( const String &filename ) = 0;
	virtual bool dirExists( const String &dirpath ) = 0;
	
	virtual UniquePtr<List<Entry>> readDir( const String &path, bool absolutePaths, bool recursive ) = 0;
	
	virtual bool mstat( MetaStat *result, const String &path ) = 0;

	virtual UniquePtr<File> openForReadingWithPlainMeta( const String &filename, const prism::fs_meta_plain_t &plainMetaValues, bool *outFileExists = nullptr );

	inline String root( const String &path )
	{
		const String rootPath = root();
		return rootPath.empty() ? path : rootPath + trimSlashesAtBegin( path );
	}
};

class FileSystem::Entry
{
public:
	Entry()
	{
	}

	Entry(String path, bool directory, bool encrypted, FileSystem *filesystem)
		: m_path(path)
		, m_directory(directory)
		, m_encrypted(encrypted)
		, m_filesystem(filesystem)
	{
	}

	inline const String &GetPath() const { return m_path; }
	inline void SetPath(const String path) { m_path = path; }

	inline bool IsDirectory() const { return m_directory; }
	inline bool IsEncrypted() const { return m_encrypted; }
	inline FileSystem *GetFileSystem() const { return m_filesystem; }

private:
	String m_path;
	bool m_directory = false;
	bool m_encrypted = false;
	FileSystem *m_filesystem = nullptr;
};

class MetaStat
{
public:
	class Meta
	{
	private:
		static constexpr u32 c_valueCapacity = 13;

	public:
		const prism::token_t &name() const { return m_name; }
		
		u32 count() const { return m_count; }

		template< typename T >
		const T &value() const
		{
			static_assert( std::is_same<decltype( T::m_value ), u32[ std::extent<decltype( T::m_value )>::value ]>::value, "Unexpected type given." );
			assert( std::extent<decltype( T::m_value )>::value == m_count );
			return reinterpret_cast<const T &>( m_value );
		}

		template< typename T >
		void setValue( const T &newValue )
		{
			static_assert( std::is_same<decltype( T::m_value ), u32[ std::extent<decltype( T::m_value )>::value ]>::value, "Unexpected type given." );
			static_assert( std::extent<decltype( T::m_value )>::value <= c_valueCapacity, "Capacity needs to be increased." );
			m_count = std::extent<decltype( T::m_value )>::value;
			memcpy( m_value, newValue.m_value, sizeof( u32 ) * m_count );
		}

	public:
		prism::token_t m_name;
		u32 m_count = 0;
		u32 m_value[ c_valueCapacity ];
	};

	FileSystem *m_filesystem = nullptr;

	Array< Meta > m_meta;

	Meta *find( prism::token_t metaName );
	const Meta *find( prism::token_t metaName ) const;

	template< typename T >
	const T &get() const
	{
		const Meta *const meta = find( prism::token_t( T::c_name ) );
		assert( meta != nullptr );
		return meta->value< T >();
	}
};

constexpr FileSystem::FsOpenMode operator|(const FileSystem::FsOpenMode t, const FileSystem::FsOpenMode f)
{
	return static_cast<FileSystem::FsOpenMode>((unsigned)t | (unsigned)f);
}

SysFileSystem *getSFS();
UberFileSystem *getUFS();

FileSystem *ufsMount(const String &root, scs_bool readOnly, int priority);
void ufsUnmount(FileSystem *fs);

/* eof */
