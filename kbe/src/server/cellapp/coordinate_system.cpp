// Copyright 2008-2018 Yolo Technologies, Inc. All Rights Reserved. https://www.comblockengine.com
#include "coordinate_node.h"
#include "coordinate_system.h"
#include "profile.h"

#ifndef CODE_INLINE
#include "coordinate_system.inl"
#endif

namespace KBEngine{	

// 静态成员初始化 - 是否启用Y轴处理
bool CoordinateSystem::hasY = false;

//-------------------------------------------------------------------------------------
/**
 * 构造函数 - 初始化坐标系统
 */
CoordinateSystem::CoordinateSystem():
size_(0),
first_x_coordinateNode_(NULL),
first_y_coordinateNode_(NULL),
first_z_coordinateNode_(NULL),
dels_(),
dels_count_(0),
updating_(0),
releases_()
{
}

//-------------------------------------------------------------------------------------
/**
 * 析构函数 - 清理所有资源
 * 遍历所有链表节点并释放内存
 */
CoordinateSystem::~CoordinateSystem()
{
	dels_.clear();
	dels_count_ = 0;

	// 遍历X轴链表，删除所有节点
	if(first_x_coordinateNode_)
	{
		CoordinateNode* pNode = first_x_coordinateNode_;
		while(pNode != NULL)
		{
			CoordinateNode* pNextNode = pNode->pNextX();

			if (pNextNode)
				pNextNode->pPrevX(NULL);

			// 清理节点的所有链表指针
			pNode->pCoordinateSystem(NULL);
			pNode->pPrevX(NULL);
			pNode->pNextX(NULL);
			pNode->pPrevY(NULL);
			pNode->pNextY(NULL);
			pNode->pPrevZ(NULL);
			pNode->pNextZ(NULL);

			delete pNode;

			pNode = pNextNode;
		}
		
		// 上面已经销毁过了，置空指针
		first_x_coordinateNode_ = NULL;
		first_y_coordinateNode_ = NULL;
		first_z_coordinateNode_ = NULL;
	}

	releaseNodes();
}

//-------------------------------------------------------------------------------------
/**
 * 插入节点到坐标系统
 * 算法思路：
 * 1. 如果是空系统，直接设置为第一个节点
 * 2. 否则将节点插入到各轴链表的头部，然后通过update方法重新排序
 */
bool CoordinateSystem::insert(CoordinateNode* pNode)
{
	// 如果链表是空的, 初始第一个和最后一个xz节点为该节点
	if(isEmpty())
	{
		first_x_coordinateNode_ = pNode;

		if(CoordinateSystem::hasY)
			first_y_coordinateNode_ = pNode;

		first_z_coordinateNode_ = pNode;

		// 初始化节点的链表指针
		pNode->pPrevX(NULL);
		pNode->pNextX(NULL);
		pNode->pPrevY(NULL);
		pNode->pNextY(NULL);
		pNode->pPrevZ(NULL);
		pNode->pNextZ(NULL);
		
		// 设置节点坐标
		pNode->x(pNode->xx());
		pNode->y(pNode->yy());
		pNode->z(pNode->zz());
		pNode->pCoordinateSystem(this);

		size_ = 1;
		
		// 只有一个节点不需要更新
		// update(pNode);
		pNode->resetOld();
		return true;
	}

	// 设置旧坐标为最小值，强制触发更新
	pNode->old_xx(-FLT_MAX);
	pNode->old_yy(-FLT_MAX);
	pNode->old_zz(-FLT_MAX);

	// 插入到X轴链表头部
	pNode->x(first_x_coordinateNode_->x());
	first_x_coordinateNode_->pPrevX(pNode);
	pNode->pNextX(first_x_coordinateNode_);
	first_x_coordinateNode_ = pNode;

	// 如果启用Y轴，插入到Y轴链表头部
	if(CoordinateSystem::hasY)
	{
		pNode->y(first_y_coordinateNode_->y());
		first_y_coordinateNode_->pPrevY(pNode);
		pNode->pNextY(first_y_coordinateNode_);
		first_y_coordinateNode_ = pNode;
	}
	
	// 插入到Z轴链表头部
	pNode->z(first_z_coordinateNode_->z());
	first_z_coordinateNode_->pPrevZ(pNode);
	pNode->pNextZ(first_z_coordinateNode_);
	first_z_coordinateNode_ = pNode;

	pNode->pCoordinateSystem(this);
	++size_;

	// 通过update方法将节点移动到正确位置
	update(pNode);
	return true;
}

//-------------------------------------------------------------------------------------
/**
 * 移除节点（延迟删除）
 * 为了避免在更新过程中立即删除节点导致的问题，采用延迟删除策略
 */
bool CoordinateSystem::remove(CoordinateNode* pNode)
{
	// 标记节点为正在移除状态
	pNode->addFlags(COORDINATE_NODE_FLAG_REMOVING);
	pNode->onRemove();
	update(pNode);
	
	// 标记节点为已移除状态
	pNode->addFlags(COORDINATE_NODE_FLAG_REMOVED);

	// 由于在update过程中可能会因为多级update的进行导致COORDINATE_NODE_FLAG_PENDING标志被取消，因此此处并不能很好的判断
	// 除非实现了标记的计数器，这里强制所有的行为都放入dels_， 由releaseNodes在space中进行调用统一释放
	if(true /*pNode->hasFlags(COORDINATE_NODE_FLAG_PENDING)*/)
	{
		// 检查节点是否已经在删除列表中
		std::list<CoordinateNode*>::iterator iter = std::find(dels_.begin(), dels_.end(), pNode);
		if(iter == dels_.end())
		{
			dels_.push_back(pNode);
			++dels_count_;
		}
	}
	else
	{
		removeReal(pNode);
	}

	return true;
}

//-------------------------------------------------------------------------------------
/**
 * 清理所有标记为删除的节点
 */
void CoordinateSystem::removeDelNodes()
{
	if(dels_count_ == 0)
		return;

	std::list<CoordinateNode*>::iterator iter = dels_.begin();
	for(; iter != dels_.end(); ++iter)
	{
		removeReal((*iter));
	}

	dels_.clear();
	dels_count_ = 0;
}

//-------------------------------------------------------------------------------------
/**
 * 释放所有待释放的节点内存
 */
void CoordinateSystem::releaseNodes()
{
	removeDelNodes();

	std::list<CoordinateNode*>::iterator iter = releases_.begin();
	for (; iter != releases_.end(); ++iter)
	{
		delete (*iter);
	}

	releases_.clear();
}

//-------------------------------------------------------------------------------------
/**
 * 真正移除节点（立即执行）
 * 从三个轴的链表中彻底移除节点
 */
bool CoordinateSystem::removeReal(CoordinateNode* pNode)
{
	if(pNode->pCoordinateSystem() == NULL)
	{
		return true;
	}

	// 从X轴链表中移除
	if(first_x_coordinateNode_ == pNode)
	{
		// 如果是第一个节点，更新头指针
		first_x_coordinateNode_ = first_x_coordinateNode_->pNextX();

		if(first_x_coordinateNode_)
		{
			first_x_coordinateNode_->pPrevX(NULL);
		}
	}
	else
	{
		// 从链表中间移除，重新连接前后节点
		pNode->pPrevX()->pNextX(pNode->pNextX());

		if(pNode->pNextX())
			pNode->pNextX()->pPrevX(pNode->pPrevX());
	}

	// 从Y轴链表中移除（如果启用Y轴）
	if(CoordinateSystem::hasY)
	{
		if(first_y_coordinateNode_ == pNode)
		{
			first_y_coordinateNode_ = first_y_coordinateNode_->pNextY();

			if(first_y_coordinateNode_)
			{
				first_y_coordinateNode_->pPrevY(NULL);
			}
		}
		else
		{
			pNode->pPrevY()->pNextY(pNode->pNextY());

			if(pNode->pNextY())
				pNode->pNextY()->pPrevY(pNode->pPrevY());
		}
	}

	// 从Z轴链表中移除
	if(first_z_coordinateNode_ == pNode)
	{
		first_z_coordinateNode_ = first_z_coordinateNode_->pNextZ();

		if(first_z_coordinateNode_)
		{
			first_z_coordinateNode_->pPrevZ(NULL);
		}
	}
	else
	{
		pNode->pPrevZ()->pNextZ(pNode->pNextZ());

		if(pNode->pNextZ())
			pNode->pNextZ()->pPrevZ(pNode->pPrevZ());
	}

	// 清理节点的所有指针
	pNode->pPrevX(NULL);
	pNode->pNextX(NULL);
	pNode->pPrevY(NULL);
	pNode->pNextY(NULL);
	pNode->pPrevZ(NULL);
	pNode->pNextZ(NULL);
	pNode->pCoordinateSystem(NULL);
	
	// 加入释放列表，稍后统一释放内存
	releases_.push_back(pNode);

	--size_;
	return true;
}

//-------------------------------------------------------------------------------------
/**
 * 在X轴上移动节点
 * 算法思路：通过比较节点位置，将节点在有序链表中移动到正确位置
 * 同时触发空间事件回调（AOI相关）
 */
void CoordinateSystem::moveNodeX(CoordinateNode* pNode, float px, CoordinateNode* pCurrNode)
{
	if (pCurrNode != NULL)
	{
		pNode->x(pCurrNode->x());

#ifdef DEBUG_COORDINATE_SYSTEM
		DEBUG_MSG(fmt::format("CoordinateSystem::update start: [{}X] ({}), pCurrNode=>({})\n",
			(pNode->pPrevX() == pCurrNode ? "-" : "+"), pNode->c_str(), pCurrNode->c_str()));
#endif

		// 判断移动方向：向前移动（坐标减小）
		if (pNode->pPrevX() == pCurrNode)
		{
			KBE_ASSERT(pCurrNode->x() >= px);

			// 重新连接链表
			CoordinateNode* pPreNode = pCurrNode->pPrevX();
			pCurrNode->pPrevX(pNode);
			if (pPreNode)
			{
				pPreNode->pNextX(pNode);
				if (pNode == first_x_coordinateNode_ && pNode->pNextX())
					first_x_coordinateNode_ = pNode->pNextX();
			}
			else
			{
				first_x_coordinateNode_ = pNode;
			}

			if (pNode->pPrevX())
				pNode->pPrevX()->pNextX(pNode->pNextX());

			if (pNode->pNextX())
				pNode->pNextX()->pPrevX(pNode->pPrevX());

			pNode->pPrevX(pPreNode);
			pNode->pNextX(pCurrNode);
		}
		else  // 向后移动（坐标增大）
		{
			KBE_ASSERT(pCurrNode->x() <= px);

			CoordinateNode* pNextNode = pCurrNode->pNextX();
			if (pNextNode != pNode)
			{
				pCurrNode->pNextX(pNode);
				if (pNextNode)
					pNextNode->pPrevX(pNode);

				if (pNode->pPrevX())
					pNode->pPrevX()->pNextX(pNode->pNextX());

				if (pNode->pNextX())
				{
					pNode->pNextX()->pPrevX(pNode->pPrevX());

					if (pNode == first_x_coordinateNode_)
						first_x_coordinateNode_ = pNode->pNextX();
				}

				pNode->pPrevX(pCurrNode);
				pNode->pNextX(pNextNode);
			}
		}

		// 触发空间事件回调（AOI系统）
		if (!pNode->hasFlags(COORDINATE_NODE_FLAG_HIDE_OR_REMOVED))
		{
#ifdef DEBUG_COORDINATE_SYSTEM
			DEBUG_MSG(fmt::format("CoordinateSystem::update1: [{}X] ({}), passNode=>({})\n",
				(pNode->pPrevX() == pCurrNode ? "-" : "+"), pNode->c_str(), pCurrNode->c_str()));
#endif

			pCurrNode->onNodePassX(pNode, true);
		}

		if (!pCurrNode->hasFlags(COORDINATE_NODE_FLAG_HIDE_OR_REMOVED))
		{
#ifdef DEBUG_COORDINATE_SYSTEM
			DEBUG_MSG(fmt::format("CoordinateSystem::update2: [{}X] ({}), passNode=>({})\n",
				(pNode->pPrevX() == pCurrNode ? "-" : "+"), pNode->c_str(), pCurrNode->c_str()));
#endif

			pNode->onNodePassX(pCurrNode, false);
		}

#ifdef DEBUG_COORDINATE_SYSTEM
		DEBUG_MSG(fmt::format("CoordinateSystem::update end: [{}X] ({}), pCurrNode=>({})\n",
			(pNode->pPrevX() == pCurrNode ? "-" : "+"), pNode->c_str(), pCurrNode->c_str()));
#endif
	}
}

//-------------------------------------------------------------------------------------
/**
 * 在Y轴上移动节点（逻辑与moveNodeX类似）
 */
void CoordinateSystem::moveNodeY(CoordinateNode* pNode, float py, CoordinateNode* pCurrNode)
{
	if (pCurrNode != NULL)
	{
		pNode->y(pCurrNode->y());

#ifdef DEBUG_COORDINATE_SYSTEM
		DEBUG_MSG(fmt::format("CoordinateSystem::update start: [{}Y] ({}), pCurrNode=>({})\n",
			(pNode->pPrevY() == pCurrNode ? "-" : "+"), pNode->c_str(), pCurrNode->c_str()));
#endif

		if (pNode->pPrevY() == pCurrNode)
		{
			KBE_ASSERT(pCurrNode->y() >= py);

			CoordinateNode* pPreNode = pCurrNode->pPrevY();
			pCurrNode->pPrevY(pNode);
			if (pPreNode)
			{
				pPreNode->pNextY(pNode);
				if (pNode == first_y_coordinateNode_ && pNode->pNextY())
					first_y_coordinateNode_ = pNode->pNextY();
			}
			else
			{
				first_y_coordinateNode_ = pNode;
			}

			if (pNode->pPrevY())
				pNode->pPrevY()->pNextY(pNode->pNextY());

			if (pNode->pNextY())
				pNode->pNextY()->pPrevY(pNode->pPrevY());

			pNode->pPrevY(pPreNode);
			pNode->pNextY(pCurrNode);
		}
		else
		{
			KBE_ASSERT(pCurrNode->y() <= py);

			CoordinateNode* pNextNode = pCurrNode->pNextY();
			if (pNextNode != pNode)
			{
				pCurrNode->pNextY(pNode);
				if (pNextNode)
					pNextNode->pPrevY(pNode);

				if (pNode->pPrevY())
					pNode->pPrevY()->pNextY(pNode->pNextY());

				if (pNode->pNextY())
				{
					pNode->pNextY()->pPrevY(pNode->pPrevY());

					if (pNode == first_y_coordinateNode_)
						first_y_coordinateNode_ = pNode->pNextY();
				}

				pNode->pPrevY(pCurrNode);
				pNode->pNextY(pNextNode);
			}
		}

		if (!pNode->hasFlags(COORDINATE_NODE_FLAG_HIDE_OR_REMOVED))
		{
#ifdef DEBUG_COORDINATE_SYSTEM
			DEBUG_MSG(fmt::format("CoordinateSystem::update1: [{}Y] ({}), passNode=>({})\n",
				(pNode->pPrevY() == pCurrNode ? "-" : "+"), pNode->c_str(), pCurrNode->c_str()));
#endif

			pCurrNode->onNodePassY(pNode, true);
		}

		if (!pCurrNode->hasFlags(COORDINATE_NODE_FLAG_HIDE_OR_REMOVED))
		{
#ifdef DEBUG_COORDINATE_SYSTEM
			DEBUG_MSG(fmt::format("CoordinateSystem::update2: [{}Y] ({}), passNode=>({})\n",
				(pNode->pPrevY() == pCurrNode ? "-" : "+"), pNode->c_str(), pCurrNode->c_str()));
#endif

			pNode->onNodePassY(pCurrNode, false);
		}

#ifdef DEBUG_COORDINATE_SYSTEM
		DEBUG_MSG(fmt::format("CoordinateSystem::update end: [{}Y] ({}), pCurrNode=>({})\n",
			(pNode->pPrevY() == pCurrNode ? "-" : "+"), pNode->c_str(), pCurrNode->c_str()));
#endif
	}
}

//-------------------------------------------------------------------------------------
/**
 * 在Z轴上移动节点（逻辑与moveNodeX类似）
 */
void CoordinateSystem::moveNodeZ(CoordinateNode* pNode, float pz, CoordinateNode* pCurrNode)
{
	if (pCurrNode != NULL)
	{
		pNode->z(pCurrNode->z());

#ifdef DEBUG_COORDINATE_SYSTEM
		DEBUG_MSG(fmt::format("CoordinateSystem::update start: [{}Z] ({}), pCurrNode=>({})\n",
			(pNode->pPrevZ() == pCurrNode ? "-" : "+"), pNode->c_str(), pCurrNode->c_str()));
#endif

		if (pNode->pPrevZ() == pCurrNode)
		{
			KBE_ASSERT(pCurrNode->z() >= pz);

			CoordinateNode* pPreNode = pCurrNode->pPrevZ();
			pCurrNode->pPrevZ(pNode);
			if (pPreNode)
			{
				pPreNode->pNextZ(pNode);
				if (pNode == first_z_coordinateNode_ && pNode->pNextZ())
					first_z_coordinateNode_ = pNode->pNextZ();
			}
			else
			{
				first_z_coordinateNode_ = pNode;
			}

			if (pNode->pPrevZ())
				pNode->pPrevZ()->pNextZ(pNode->pNextZ());

			if (pNode->pNextZ())
				pNode->pNextZ()->pPrevZ(pNode->pPrevZ());

			pNode->pPrevZ(pPreNode);
			pNode->pNextZ(pCurrNode);
		}
		else
		{
			KBE_ASSERT(pCurrNode->z() <= pz);

			CoordinateNode* pNextNode = pCurrNode->pNextZ();
			if (pNextNode != pNode)
			{
				pCurrNode->pNextZ(pNode);
				if (pNextNode)
					pNextNode->pPrevZ(pNode);

				if (pNode->pPrevZ())
					pNode->pPrevZ()->pNextZ(pNode->pNextZ());

				if (pNode->pNextZ())
				{
					pNode->pNextZ()->pPrevZ(pNode->pPrevZ());

					if (pNode == first_z_coordinateNode_)
						first_z_coordinateNode_ = pNode->pNextZ();
				}

				pNode->pPrevZ(pCurrNode);
				pNode->pNextZ(pNextNode);
			}
		}

		if (!pNode->hasFlags(COORDINATE_NODE_FLAG_HIDE_OR_REMOVED))
		{
#ifdef DEBUG_COORDINATE_SYSTEM
			DEBUG_MSG(fmt::format("CoordinateSystem::update1: [{}Z] ({}), passNode=>({})\n",
				(pNode->pPrevZ() == pCurrNode ? "-" : "+"), pNode->c_str(), pCurrNode->c_str()));
#endif

			pCurrNode->onNodePassZ(pNode, true);
	}

		if (!pCurrNode->hasFlags(COORDINATE_NODE_FLAG_HIDE_OR_REMOVED))
		{
#ifdef DEBUG_COORDINATE_SYSTEM
			DEBUG_MSG(fmt::format("CoordinateSystem::update2: [{}Z] ({}), passNode=>({})\n",
				(pNode->pPrevZ() == pCurrNode ? "-" : "+"), pNode->c_str(), pCurrNode->c_str()));
#endif

			pNode->onNodePassZ(pCurrNode, false);
}

#ifdef DEBUG_COORDINATE_SYSTEM
		DEBUG_MSG(fmt::format("CoordinateSystem::update end: [{}Z] ({}), pCurrNode=>({})\n",
			(pNode->pPrevZ() == pCurrNode ? "-" : "+"), pNode->c_str(), pCurrNode->c_str()));
#endif
	}
}

//-------------------------------------------------------------------------------------
/**
 * 更新节点在坐标系统中的位置
 * 这是坐标系统的核心方法，当节点位置发生变化时调用
 * 
 * 算法思路：
 * 1. 检查每个轴上的坐标是否发生变化
 * 2. 如果发生变化，通过冒泡排序的方式将节点移动到正确位置
 * 3. 在移动过程中触发相应的空间事件回调
 */
void CoordinateSystem::update(CoordinateNode* pNode)
{
	AUTO_SCOPED_PROFILE("coordinateSystemUpdates");

#ifdef DEBUG_COORDINATE_SYSTEM
	DEBUG_MSG(fmt::format("CoordinateSystem::update enter:[{:p}]:  ({}  {}  {})\n", (void*)pNode, pNode->xx(), pNode->yy(), pNode->zz()));
#endif

	// 没有计数器支持，这个标记很可能中途被update子分支取消，因此没有意义
	//pNode->addFlags(COORDINATE_NODE_FLAG_PENDING);

	++updating_;

	// 处理X轴坐标变化
	if (pNode->xx() != pNode->old_xx())
	{
		// 向前搜索（坐标减小方向）
		CoordinateNode* pCurrNode = pNode->pPrevX();
		while (pCurrNode && pCurrNode != pNode &&
			((pCurrNode->x() > pNode->xx()) ||
			(pCurrNode->x() == pNode->xx() && !pCurrNode->hasFlags(COORDINATE_NODE_FLAG_NEGATIVE_BOUNDARY))))
		{
			moveNodeX(pNode, pNode->xx(), pCurrNode);
			pCurrNode = pNode->pPrevX();
		}

		// 向后搜索（坐标增大方向）
		pCurrNode = pNode->pNextX();
		while (pCurrNode && pCurrNode != pNode &&
			((pCurrNode->x() < pNode->xx()) ||
			(pCurrNode->x() == pNode->xx() && !pCurrNode->hasFlags(COORDINATE_NODE_FLAG_POSITIVE_BOUNDARY))))
		{
			moveNodeX(pNode, pNode->xx(), pCurrNode);
			pCurrNode = pNode->pNextX();
		}

		pNode->x(pNode->xx());
	}

	// 处理Y轴坐标变化（如果启用Y轴）
	if (CoordinateSystem::hasY && pNode->yy() != pNode->old_yy())
	{
		CoordinateNode* pCurrNode = pNode->pPrevY();
		while (pCurrNode && pCurrNode != pNode &&
			((pCurrNode->y() > pNode->yy()) ||
			(pCurrNode->y() == pNode->yy() && !pCurrNode->hasFlags(COORDINATE_NODE_FLAG_NEGATIVE_BOUNDARY))))
		{
			moveNodeY(pNode, pNode->yy(), pCurrNode);
			pCurrNode = pNode->pPrevY();
		}

		pCurrNode = pNode->pNextY();
		while (pCurrNode && pCurrNode != pNode &&
			((pCurrNode->y() < pNode->yy()) ||
			(pCurrNode->y() == pNode->yy() && !pCurrNode->hasFlags(COORDINATE_NODE_FLAG_POSITIVE_BOUNDARY))))
		{
			moveNodeY(pNode, pNode->yy(), pCurrNode);
			pCurrNode = pNode->pNextY();
		}

		pNode->y(pNode->yy());
	}

	// 处理Z轴坐标变化
	if (pNode->zz() != pNode->old_zz())
	{
		CoordinateNode* pCurrNode = pNode->pPrevZ();
		while (pCurrNode && pCurrNode != pNode &&
			((pCurrNode->z() > pNode->zz()) ||
			(pCurrNode->z() == pNode->zz() && !pCurrNode->hasFlags(COORDINATE_NODE_FLAG_NEGATIVE_BOUNDARY))))
		{
			moveNodeZ(pNode, pNode->zz(), pCurrNode);
			pCurrNode = pNode->pPrevZ();
		}

		pCurrNode = pNode->pNextZ();
		while (pCurrNode && pCurrNode != pNode &&
			((pCurrNode->z() < pNode->zz()) ||
			(pCurrNode->z() == pNode->zz() && !pCurrNode->hasFlags(COORDINATE_NODE_FLAG_POSITIVE_BOUNDARY))))
		{
			moveNodeZ(pNode, pNode->zz(), pCurrNode);
			pCurrNode = pNode->pNextZ();
		}

		pNode->z(pNode->zz());
	}

	// 重置旧坐标，完成更新
	pNode->resetOld();
	//pNode->removeFlags(COORDINATE_NODE_FLAG_PENDING);
	--updating_;

	//if (updating_ == 0)
	//	releaseNodes();

#ifdef DEBUG_COORDINATE_SYSTEM
	DEBUG_MSG(fmt::format("CoordinateSystem::debugX[ x ]:[{:p}]\n", (void*)pNode));
	first_x_coordinateNode_->debugX();
	DEBUG_MSG(fmt::format("CoordinateSystem::debugY[ y ]:[{:p}]\n", (void*)pNode));
	if (first_y_coordinateNode_)first_y_coordinateNode_->debugY();
	DEBUG_MSG(fmt::format("CoordinateSystem::debugZ[ z ]:[{:p}]\n", (void*)pNode));
	first_z_coordinateNode_->debugZ();
#endif
}

//-------------------------------------------------------------------------------------
}
