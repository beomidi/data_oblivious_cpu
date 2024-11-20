package boom.taint

import chisel3._
import chisel3.util._

import org.chipsalliance.cde.config.Parameters

import boom.util._
import boom.common._


class TaintIO(val exe_length: Int)(implicit p: Parameters) extends BoomBundle
{
  // incoming microops from rename2
  val exe_tlb_uop        = Input((Vec(memWidth, new MicroOp()))) 
  val tlb_taint          = Input(Vec(memWidth, Valid(Bool())))     
  val dis_uops           = Vec(coreWidth, Flipped(DecoupledIO(new MicroOp())))
 // val issue_uops        =  Vec(exe_length, Flipped(DecoupledIO(new MicroOp())))
  val commit_uops        = Vec(retireWidth, Flipped(DecoupledIO(new MicroOp())))
  val commit_flush       = Input(Bool())
  val rename_taints_o      = Output(UInt((numFpPhysRegs + numIntPhysRegs).W))
  val commit_taints_i    = Input(UInt(logicalRegCount.W))
  val commit_taints_o      = Output(UInt(logicalRegCount.W))
}


class Taint(val exe_length: Int)(implicit p: Parameters) extends BoomModule {
  val io = IO(new TaintIO(exe_length))
  val rename_taint_vec = RegInit(0.U((numFpPhysRegs + numIntPhysRegs).W))
  val commit_taint_vec = RegInit(0.U(logicalRegCount.W))

  val rename_taint_vec_tmp = Wire(Vec(coreWidth + 1, UInt((numFpPhysRegs + numIntPhysRegs).W)))
  val rename_taint_vec_lrs_to_prs = Wire(Vec(coreWidth, Vec(3, UInt((numFpPhysRegs + numIntPhysRegs).W))))
  val commit_taint_vec_tmp = Wire(Vec(retireWidth + 1, UInt(logicalRegCount.W)))


  dontTouch(rename_taint_vec)
  dontTouch(commit_taint_vec)

  // val taint_queue = Module(new Queue((new MicroOp()),  numRobEntries))
  // taint_queue.io.enq.valid := io.rename_uops(0).valid
  // taint_queue.io.enq.bits := io.rename_uops(0).bits
  // taint_queue.io.deq.ready := true.B
  // val queue_out = taint_queue.io.deq
  // dontTouch(queue_out)

  def getTaint(taint_vec: UInt, reg : UInt): Bool = {
      taint_vec(reg).asBool()
  }

  def setTaint(taint_vec: UInt, reg : UInt, taint : Bool): UInt = {
      taint_vec.bitSet(reg, taint)
  }


  rename_taint_vec_tmp(0) := rename_taint_vec
  for (w <- 1 until coreWidth + 1) {
    rename_taint_vec_tmp(w) := rename_taint_vec
    rename_taint_vec_lrs_to_prs(w-1)(0) := rename_taint_vec
    rename_taint_vec_lrs_to_prs(w-1)(1) := rename_taint_vec
    rename_taint_vec_lrs_to_prs(w-1)(2) := rename_taint_vec
    when(io.dis_uops(w - 1).valid && io.dis_uops(w - 1).bits.ldst_val){
         when(io.dis_uops(w - 1).bits.lrs1_rtype === RT_FIX && io.dis_uops(w - 1).bits.lrs2_rtype =/= RT_FIX){
          rename_taint_vec_tmp(w) := setTaint(rename_taint_vec_tmp(w - 1), io.dis_uops(w - 1).bits.pdst, 
              getTaint(Mux(io.dis_uops(w - 1).bits.prs1_busy, rename_taint_vec_tmp(w - 1), commit_taint_vec), 
              Mux(io.dis_uops(w - 1).bits.prs1_busy, io.dis_uops(w - 1).bits.prs1, io.dis_uops(w - 1).bits.lrs1))) & 
              Mux(io.dis_uops(w - 1).bits.prs1_busy, setTaint(rename_taint_vec_lrs_to_prs(w-1)(0), io.dis_uops(w - 1).bits.prs1, getTaint(rename_taint_vec, io.dis_uops(w - 1).bits.prs1)), 
               setTaint(rename_taint_vec_lrs_to_prs(w-1)(0), io.dis_uops(w - 1).bits.prs1, getTaint(commit_taint_vec, io.dis_uops(w - 1).bits.lrs1)))
              
          }otherwise{
            when(io.dis_uops(w - 1).bits.lrs1_rtype === RT_FIX && io.dis_uops(w - 1).bits.lrs2_rtype === RT_FIX){
              rename_taint_vec_tmp(w) := setTaint(rename_taint_vec_tmp(w - 1), io.dis_uops(w - 1).bits.pdst, 
                getTaint(Mux(io.dis_uops(w - 1).bits.prs1_busy, rename_taint_vec_tmp(w - 1), commit_taint_vec), 
                Mux(io.dis_uops(w - 1).bits.prs1_busy, io.dis_uops(w - 1).bits.prs1, io.dis_uops(w - 1).bits.lrs1)) |
                getTaint(Mux(io.dis_uops(w - 1).bits.prs2_busy, rename_taint_vec_tmp(w - 1), commit_taint_vec), 
                Mux(io.dis_uops(w - 1).bits.prs2_busy, io.dis_uops(w - 1).bits.prs2, io.dis_uops(w - 1).bits.lrs2))) & 
                Mux(io.dis_uops(w - 1).bits.prs1_busy, setTaint(rename_taint_vec_lrs_to_prs(w-1)(0), io.dis_uops(w - 1).bits.prs1, getTaint(rename_taint_vec, io.dis_uops(w - 1).bits.prs1)), 
                setTaint(rename_taint_vec_lrs_to_prs(w-1)(0), io.dis_uops(w - 1).bits.prs1, getTaint(commit_taint_vec, io.dis_uops(w - 1).bits.lrs1)))&
                Mux(io.dis_uops(w - 1).bits.prs2_busy, setTaint(rename_taint_vec_lrs_to_prs(w-1)(1), io.dis_uops(w - 1).bits.prs2, getTaint(rename_taint_vec, io.dis_uops(w - 1).bits.prs2)), 
                setTaint(rename_taint_vec_lrs_to_prs(w-1)(1), io.dis_uops(w - 1).bits.prs2, getTaint(commit_taint_vec, io.dis_uops(w - 1).bits.lrs2)))
              } otherwise{
                rename_taint_vec_tmp(w) := setTaint(rename_taint_vec_tmp(w - 1), io.dis_uops(w - 1).bits.pdst, false.B)
            }
          }
        }
    }
  rename_taint_vec := rename_taint_vec_tmp(coreWidth)
   

  commit_taint_vec_tmp(0) := commit_taint_vec
  for (w <- 1 until retireWidth + 1) {
      commit_taint_vec_tmp(w) := commit_taint_vec
      when(io.commit_uops(w - 1).valid){
        when(io.commit_uops(w - 1).bits.uopc === uopSTA || io.dis_uops(w - 1).bits.uopc === uopSTD){
          commit_taint_vec_tmp(w) := setTaint(commit_taint_vec_tmp(w - 1), io.commit_uops(w - 1).bits.lrs2, false.B)
        }otherwise{
          commit_taint_vec_tmp(w) := setTaint(commit_taint_vec_tmp(w - 1), io.commit_uops(w - 1).bits.ldst, getTaint(rename_taint_vec, io.commit_uops(w - 1).bits.pdst))
        }
        //  printf("uop = (0x%x), inst = (0x%x), pdstReg = (0x%x), prs1Reg = (0x%x), prs2Reg = (0x%x), prs3Reg = (0x%x), rename_taint_table = (0x%x)\n", io.commit_uops(w - 1).bits.uopc, io.commit_uops(w - 1).bits.inst,
        //  io.commit_uops(w - 1).bits.pdst, io.commit_uops(w - 1).bits.prs1, io.commit_uops(w - 1).bits.prs2, io.commit_uops(w - 1).bits.prs3, this.rename_taint_vec)

        // printf("uop = (0x%x), inst = (0x%x), ldstReg = (0x%x), lrs1Reg = (0x%x), lrs2Reg = (0x%x), lrs3Reg = (0x%x), commit_taint_table = (0x%x)\n", io.commit_uops(w - 1).bits.uopc, io.commit_uops(w - 1).bits.inst,
        //  io.commit_uops(w - 1).bits.ldst, io.commit_uops(w - 1).bits.lrs1, io.commit_uops(w - 1).bits.lrs2, io.commit_uops(w - 1).bits.lrs3, this.commit_taint_vec)
        //printf("ldstReg = %d, pdstReg = %d, commit_inst = (0x%x), commit_taint_table = (0x%x)\n", io.commit_uops(w - 1).bits.ldst, io.commit_uops(w - 1).bits.pdst, io.commit_uops(w - 1).bits.inst, commit_taint_vec)
      } 
  }
  commit_taint_vec := commit_taint_vec_tmp(retireWidth)



  //TODO- change to support memWidth
  when(io.tlb_taint(0).valid){
     rename_taint_vec := setTaint(rename_taint_vec, io.exe_tlb_uop(0).pdst, io.tlb_taint(0).bits)
     //printf("io.exe_tlb_uop(0).pdst = %d\n", io.exe_tlb_uop(0).pdst)
   }

  when(io.commit_flush){
      rename_taint_vec := 0.U
      //printf("rob flush\n")
  }
    

   when(io.commit_uops(0).valid && (io.commit_uops(0).bits.inst === 0x30200073.U || io.commit_uops(0).bits.inst === 0x10200073.U)){
    commit_taint_vec := io.commit_taints_i
   }

   io.commit_taints_o := commit_taint_vec
   io.rename_taints_o := rename_taint_vec

  //  when(io.dis_uops(0).valid){
  //   printf("prs1_busy = %d, prs2_busy = %d, pdstReg = (0x%x), prs1Reg = (0x%x), prs2Reg = (0x%x), dis_inst = (0x%x), rename_taint_table = (0x%x)\n", io.dis_uops(0).bits.prs1_busy, io.dis_uops(0).bits.prs2_busy, 
  //       io.dis_uops(0).bits.pdst, io.dis_uops(0).bits.prs1, io.dis_uops(0).bits.prs2, io.dis_uops(0).bits.inst, this.rename_taint_vec)
  //  }
  //for(w <- 0 until exe_length){
    //io.taint(w) := Mux(io.issue_uops(w).valid, getTaint(rename_taint_vec, io.issue_uops(w).bits.pdst), false.B)
 // }

//  io.rename_taints_o := 0xFFFFFF.U
//  io.commit_taints_o := 0.U
}
