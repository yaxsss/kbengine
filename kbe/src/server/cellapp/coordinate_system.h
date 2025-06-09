// Copyright 2008-2018 Yolo Technologies, Inc. All Rights Reserved. https://www.comblockengine.com

#ifndef KBE_COORDINATE_SYSTEM_H
#define KBE_COORDINATE_SYSTEM_H

#include "helper/debug_helper.h"
#include "common/common.h"	

//#define DEBUG_COORDINATE_SYSTEM

namespace KBEngine{

class CoordinateNode;

/**
 * 坐标系统类 - 管理游戏世界中实体的空间位置
 * 
 * 该类维护三个有序的双向链表（X、Y、Z轴），用于高效地管理和查询空间中的节点。
 * 当节点位置发生变化时，系统会自动重新排序并触发相应的空间事件回调。
 * 这种设计使得空间查询（如AOI - Area of Interest）变得非常高效。
 */
class CoordinateSystem
{
public:
	CoordinateSystem();
	~CoordinateSystem();

	/**
	 * 将节点插入到坐标系统中
	 * @param pNode 要插入的坐标节点
	 * @return 插入是否成功
	 */
	bool insert(CoordinateNode* pNode);

	/**
	 * 从坐标系统中移除节点（延迟删除）
	 * @param pNode 要移除的坐标节点
	 * @return 移除是否成功
	 */
	bool remove(CoordinateNode* pNode);
	
	/**
	 * 立即从坐标系统中移除节点
	 * @param pNode 要移除的坐标节点
	 * @return 移除是否成功
	 */
	bool removeReal(CoordinateNode* pNode);
	
	/**
	 * 清理所有标记为删除的节点
	 */
	void removeDelNodes();
	
	/**
	 * 释放所有待释放的节点内存
	 */
	void releaseNodes();

	/**
	 * 更新节点在坐标系统中的位置
	 * 当节点坐标发生变化时调用，会重新排序各轴上的链表
	 * @param pNode 需要更新的坐标节点
	 */
	void update(CoordinateNode* pNode);

	/**
	 * 在X轴上移动节点
	 * @param pNode 要移动的节点
	 * @param px 新的X坐标
	 * @param pCurrNode 当前处理的节点（用于链表重排）
	 */
	void moveNodeX(CoordinateNode* pNode, float px, CoordinateNode* pCurrNode);
	
	/**
	 * 在Y轴上移动节点
	 * @param pNode 要移动的节点
	 * @param py 新的Y坐标
	 * @param pCurrNode 当前处理的节点（用于链表重排）
	 */
	void moveNodeY(CoordinateNode* pNode, float py, CoordinateNode* pCurrNode);
	
	/**
	 * 在Z轴上移动节点
	 * @param pNode 要移动的节点
	 * @param pz 新的Z坐标
	 * @param pCurrNode 当前处理的节点（用于链表重排）
	 */
	void moveNodeZ(CoordinateNode* pNode, float pz, CoordinateNode* pCurrNode);

	/**
	 * 获取X轴链表的第一个节点
	 * @return X轴第一个节点指针
	 */
	INLINE CoordinateNode * pFirstXNode() const;
	
	/**
	 * 获取Y轴链表的第一个节点
	 * @return Y轴第一个节点指针
	 */
	INLINE CoordinateNode * pFirstYNode() const;
	
	/**
	 * 获取Z轴链表的第一个节点
	 * @return Z轴第一个节点指针
	 */
	INLINE CoordinateNode * pFirstZNode() const;

	/**
	 * 检查坐标系统是否为空
	 * @return 如果没有节点则返回true
	 */
	INLINE bool isEmpty() const;

	/**
	 * 获取坐标系统中节点的数量
	 * @return 节点总数
	 */
	INLINE uint32 size() const;

	/**
	 * 全局标志，指示是否启用Y轴处理
	 * 在某些2D游戏中可能不需要Y轴处理
	 */
	static bool hasY;

	/**
	 * 增加更新计数器（进入更新状态）
	 */
	INLINE void incUpdating();
	
	/**
	 * 减少更新计数器（退出更新状态）
	 */
	INLINE void decUpdating();

private:
	// 坐标系统中节点的总数
	uint32 size_;

	// 各轴有序链表的头节点指针
	CoordinateNode* first_x_coordinateNode_;  // X轴链表头
	CoordinateNode* first_y_coordinateNode_;  // Y轴链表头
	CoordinateNode* first_z_coordinateNode_;  // Z轴链表头

	// 延迟删除的节点列表
	std::list<CoordinateNode*> dels_;
	size_t dels_count_;  // 待删除节点数量

	// 更新状态计数器，防止在更新过程中进行某些操作
	int updating_;

	// 待释放的节点列表（用于内存管理）
	std::list<CoordinateNode*> releases_;
};

}

#ifdef CODE_INLINE
#include "coordinate_system.inl"
#endif
#endif
